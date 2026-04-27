#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/TargetLogHook.h>
#include <gwa3/game/GameTypes.h>
#include <gwa3/game/NPC.h>
#include <gwa3/game/Player.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/packets/Headers.h>

#include <Windows.h>
#include <cmath>
#include <cstring>

namespace GWA3::AgentMgr {

static constexpr uint32_t kSendCallTargetUiMessage = 0x30000013u;
static constexpr uint32_t kSendWorldActionUiMessage = 0x30000020u;
static constexpr uint32_t kActionInteractCode = 0x80u;

enum class CallTargetType : uint32_t {
  Following = 0x3,
  Morale = 0x7,
  AttackingOrTargetting = 0xA,
  None = 0xFF
};

enum class WorldActionId : uint32_t {
  InteractEnemy = 0,
  InteractPlayerOrOther = 1,
  InteractNPC = 2,
  InteractItem = 3,
  InteractTrade = 4,
  InteractGadget = 5
};

struct CallTargetPacket {
  CallTargetType call_type;
  uint32_t agent_id;
};

struct WorldActionUIPacket {
  uint32_t action_id;
  uint32_t agent_id;
  uint32_t suppress_call_target;
};

using MoveFn = void(__cdecl *)(const void *);
using ChangeTargetFn = void(__cdecl *)(uint32_t, uint32_t);
using InteractItemFn = void(__cdecl *)(uint32_t, uint32_t);
using InteractNPCFn = void(__cdecl *)(uint32_t, uint32_t);
using WorldActionFn = void(__cdecl *)(uint32_t, uint32_t, uint32_t);
using CallTargetFn = void(__cdecl *)(CallTargetType, uint32_t);

struct MoveData {
  float x;
  float y;
  uint32_t plane;
};

static MoveFn s_moveFn = nullptr;
static ChangeTargetFn s_changeTargetFn = nullptr;
static InteractItemFn s_interactItemFn = nullptr;
static InteractNPCFn s_interactNpcFn = nullptr;
static WorldActionFn s_worldActionFn = nullptr;
static CallTargetFn s_callTargetFn = nullptr;
static bool s_initialized = false;
static bool s_loggedCurrentTargetRead = false;
static bool s_loggedCurrentTargetFault = false;
static bool s_loggedTargetLogRead = false;
static bool s_loggedTargetLogStats = false;
static bool s_loggedInteractNpcNative = false;
static bool s_loggedInteractNpcFallback = false;
static bool s_loggedInteractNpcVariant = false;
static bool s_loggedInteractNpcWorldAction = false;
static bool s_loggedInteractAgentWorldAction = false;
static bool s_loggedInteractAgentWorldActionFallback = false;
static bool s_loggedLegacySignpostPath = false;
static bool s_loggedMoveQueuedOnce = false;
static bool s_loggedEngineMoveLane = false;
static bool s_loggedEngineMoveLaneUnavailable = false;
static bool s_loggedChangeTargetNativeLane = false;
static bool s_loggedChangeTargetEngineLane = false;
static SRWLOCK s_moveQueueLock = SRWLOCK_INIT;
static MoveData s_pendingQueuedMove{};
static bool s_pendingQueuedMoveValid = false;
static volatile LONG s_moveDrainQueued = 0;
static MoveData s_lastIssuedMove{};
static DWORD s_lastIssuedMoveAt = 0;
static bool s_haveLastIssuedMove = false;
static constexpr LONG kRenderCommandSlots = 32;
static constexpr size_t kRenderCommandSlotSize = 32;
static uintptr_t s_renderCommandPool = 0;
static volatile LONG s_renderCommandSlotIndex = 0;

static uintptr_t FindNearCallTarget(uintptr_t center, int backward,
                                    int forward) {
  if (!center)
    return 0;
  uintptr_t start =
      center > static_cast<uintptr_t>(backward) ? center - backward : center;
  uintptr_t end = center + forward;
  for (uintptr_t p = start; p <= end; ++p) {
    __try {
      if (*reinterpret_cast<uint8_t *>(p) != 0xE8)
        continue;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      continue;
    }
    uintptr_t fn = Scanner::FunctionFromNearCall(p);
    if (fn > 0x10000)
      return fn;
  }
  return 0;
}

bool Initialize() {
  if (s_initialized)
    return true;

  if (Offsets::Move)
    s_moveFn = reinterpret_cast<MoveFn>(Offsets::Move);
  if (Offsets::ChangeTarget)
    s_changeTargetFn = reinterpret_cast<ChangeTargetFn>(Offsets::ChangeTarget);
  uintptr_t interactAgentCall =
      Scanner::Find("\xC7\x45\xF0\x98\x3A\x00\x00", "xxxxxxx", 0x41);
  if (interactAgentCall) {
    uintptr_t interactAgentFn = FindNearCallTarget(interactAgentCall, 8, 8);
    if (interactAgentFn) {
      uintptr_t callTargetFn = FindNearCallTarget(interactAgentFn + 0xD6, 8, 8);
      if (callTargetFn) {
        s_callTargetFn = reinterpret_cast<CallTargetFn>(callTargetFn);
      }
      uintptr_t interactItemFn =
          FindNearCallTarget(interactAgentFn + 0xF8, 8, 8);
      if (!interactItemFn) {
        interactItemFn = FindNearCallTarget(interactAgentFn + 0xF0, 24, 24);
      }
      if (interactItemFn) {
        s_interactItemFn = reinterpret_cast<InteractItemFn>(interactItemFn);
      }
      uintptr_t interactNpcFn =
          FindNearCallTarget(interactAgentFn + 0xE7, 24, 24);
      if (interactNpcFn) {
        s_interactNpcFn = reinterpret_cast<InteractNPCFn>(interactNpcFn);
      }
      Log::Info(
          "AgentMgr: Interact scan anchor=0x%08X agentFn=0x%08X itemFn=0x%08X",
          static_cast<unsigned>(interactAgentCall),
          static_cast<unsigned>(interactAgentFn),
          static_cast<unsigned>(interactItemFn));
    }
  }

  // If local scan didn't find CallTarget, try the Offsets::CallTargetFunc from
  // PostProcessOffsets
  if (!s_callTargetFn && Offsets::CallTargetFunc > 0x10000) {
    s_callTargetFn = reinterpret_cast<CallTargetFn>(Offsets::CallTargetFunc);
    Log::Info(
        "AgentMgr: CallTarget resolved from Offsets::CallTargetFunc=0x%08X",
        Offsets::CallTargetFunc);
  }
  if (!s_worldActionFn && Offsets::WorldActionFunc > 0x10000) {
    s_worldActionFn = reinterpret_cast<WorldActionFn>(Offsets::WorldActionFunc);
    Log::Info(
        "AgentMgr: WorldAction resolved from Offsets::WorldActionFunc=0x%08X",
        Offsets::WorldActionFunc);
  }
  if (!s_interactNpcFn && Offsets::InteractNPCFunc > 0x10000) {
    s_interactNpcFn = reinterpret_cast<InteractNPCFn>(Offsets::InteractNPCFunc);
    Log::Info(
        "AgentMgr: InteractNPC resolved from Offsets::InteractNPCFunc=0x%08X",
        Offsets::InteractNPCFunc);
  }

  s_initialized = true;
  Log::Info(
      "AgentMgr: Initialized (Move=0x%08X, ChangeTarget=0x%08X, "
      "CallTarget=0x%08X, InteractNPC=0x%08X, InteractItem=0x%08X)",
      Offsets::Move, Offsets::ChangeTarget,
      static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_callTargetFn)),
      static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_interactNpcFn)),
      static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_interactItemFn)));
  return true;
}

static bool s_loggedMoveOnce = false;
struct SendChangeTargetUIMsg {
  uint32_t target_id;
  uint32_t auto_target_id;
};

namespace {

void IssueNativeMove(float x, float y);

const char *NpcInteractModeName(NpcInteractMode mode) {
  switch (mode) {
  case NpcInteractMode::WorldActionNoCallTarget:
    return "world-action-ct0";
  case NpcInteractMode::WorldActionCallTarget:
    return "world-action-ct1";
  case NpcInteractMode::NativePostCallTarget:
    return "native-post-ct1";
  case NpcInteractMode::NativePostNoCallTarget:
    return "native-post-ct0";
  case NpcInteractMode::NativePreCallTarget:
    return "native-pre-ct1";
  case NpcInteractMode::NativePreNoCallTarget:
    return "native-pre-ct0";
  case NpcInteractMode::PacketNpc8:
    return "packet-0x39-8";
  case NpcInteractMode::PacketNpc12:
    return "packet-0x39-12";
  default:
    return "unknown";
  }
}

WorldActionId ResolveWorldActionId(uint32_t agentId) {
  auto *agent = GetAgentByID(agentId);
  if (!agent) {
    return WorldActionId::InteractPlayerOrOther;
  }
  if (agent->type == 0x400u) {
    return WorldActionId::InteractItem;
  }
  if (agent->type == 0x200u) {
    return WorldActionId::InteractGadget;
  }
  if (agent->type != 0xDBu) {
    return WorldActionId::InteractPlayerOrOther;
  }

  auto *living = static_cast<AgentLiving *>(agent);
  if (living->allegiance == 3u) {
    return WorldActionId::InteractEnemy;
  }
  if (living->allegiance == 6u) {
    return WorldActionId::InteractNPC;
  }
  return WorldActionId::InteractPlayerOrOther;
}

void InvokeWorldActionRaw(uint32_t agentId, uint32_t callTarget) {
  if (!s_worldActionFn)
    return;
  const auto actionId = static_cast<uint32_t>(ResolveWorldActionId(agentId));
  s_worldActionFn(actionId, agentId, callTarget);
}

bool EnsureRenderCommandPool() {
  if (s_renderCommandPool)
    return true;

  void *mem =
      VirtualAlloc(nullptr, kRenderCommandSlots * kRenderCommandSlotSize,
                   MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  if (!mem) {
    Log::Error("AgentMgr: VirtualAlloc failed for render command pool");
    return false;
  }

  s_renderCommandPool = reinterpret_cast<uintptr_t>(mem);
  return true;
}

uintptr_t NextRenderCommandSlot() {
  const LONG idx = InterlockedIncrement(&s_renderCommandSlotIndex) - 1;
  return s_renderCommandPool + (static_cast<size_t>(idx % kRenderCommandSlots) *
                                kRenderCommandSlotSize);
}

bool IsCastingState(const AgentLiving *agent) {
  if (!agent)
    return false;
  return agent->skill != 0u || agent->model_state == 0x41u ||
         agent->model_state == 0x245u || agent->model_state == 0x645u;
}

struct MoveCommand {
  uintptr_t fn;
  MoveData move;
};

struct ChangeTargetCommand {
  uintptr_t fn;
  uint32_t agent_id;
};

__declspec(naked) void BotshubMoveCommandStub() {
  __asm {
        lea eax, dword ptr [eax+4]
        push eax
        call dword ptr [s_moveFn]
        add esp, 4
        jmp GWA3BotshubCommandReturnThunk
  }
}

__declspec(naked) void RenderMoveCommandStub() {
  __asm {
        lea eax, dword ptr [eax+4]
        push eax
        call dword ptr [s_moveFn]
        pop eax
        jmp GWA3CtoSHookCommandReturnThunk
  }
}

__declspec(naked) void BotshubChangeTargetCommandStub() {
  __asm {
        xor edx, edx
        push edx
        mov eax, dword ptr [eax+4]
        push eax
        call dword ptr [s_changeTargetFn]
        add esp, 8
        jmp GWA3BotshubCommandReturnThunk
  }
}

__declspec(naked) void RenderChangeTargetCommandStub() {
  __asm {
        xor edx, edx
        push edx
        mov eax, dword ptr [eax+4]
        push eax
        call dword ptr [s_changeTargetFn]
        add esp, 8
        jmp GWA3CtoSHookCommandReturnThunk
  }
}

void InvokeNativeMoveRaw(const MoveData *move) {
  if (!s_moveFn || !move)
    return;
  // Direct C function pointer call — matches GWCA's Move_Func(&pos).
  s_moveFn(move);
}

void InvokeChangeTargetRaw(uint32_t agentId) {
  if (!s_changeTargetFn)
    return;
  // Direct C function pointer call — the inline asm version was suspected
  // of stack corruption (same pattern as the Move fix at
  // InvokeNativeMoveRaw).
  __try {
    s_changeTargetFn(agentId, 0u);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    Log::Error("AgentMgr: InvokeChangeTargetRaw exception 0x%08X agentId=%u",
               GetExceptionCode(), agentId);
  }
}

void InvokeMove(void *raw) {
  if (!s_moveFn || !raw)
    return;
  auto *cmd = reinterpret_cast<MoveCommand *>(raw);
  if (!MapMgr::GetIsMapLoaded() || GetMyId() == 0) {
    return;
  }
  InvokeNativeMoveRaw(&cmd->move);
}

void InvokeChangeTarget(void *raw) {
  if (!s_changeTargetFn || !raw)
    return;
  const auto *cmd = reinterpret_cast<const ChangeTargetCommand *>(raw);
  InvokeChangeTargetRaw(cmd->agent_id);
}

bool ShouldSuppressMove(float x, float y) {
  if (!s_haveLastIssuedMove) {
    return false;
  }

  const float delta =
      std::hypot(x - s_lastIssuedMove.x, y - s_lastIssuedMove.y);
  return delta < 150.0f && (GetTickCount() - s_lastIssuedMoveAt) < 1500;
}

void DrainQueuedMove() {
  MoveData nextMove{};
  bool haveMove = false;

  AcquireSRWLockExclusive(&s_moveQueueLock);
  if (s_pendingQueuedMoveValid) {
    nextMove = s_pendingQueuedMove;
    s_pendingQueuedMoveValid = false;
    haveMove = true;
  }
  ReleaseSRWLockExclusive(&s_moveQueueLock);

  if (haveMove) {
    // Native movement must stay at one call per engine tick. If a newer
    // move arrives while this one is draining, schedule another post
    // rather than chaining a second native move in the same callback.
    IssueNativeMove(nextMove.x, nextMove.y);
  }

  InterlockedExchange(&s_moveDrainQueued, 0);

  AcquireSRWLockShared(&s_moveQueueLock);
  const bool needsAnotherDrain = s_pendingQueuedMoveValid;
  ReleaseSRWLockShared(&s_moveQueueLock);

  if (needsAnotherDrain &&
      InterlockedCompareExchange(&s_moveDrainQueued, 1, 0) == 0) {
    GameThread::EnqueuePost([]() { DrainQueuedMove(); });
  }
}

void IssueNativeMove(float x, float y) {
  // Safety: don't call native move during zone transitions or when agent is
  // invalid. The native fn crashes if called while the world state is being
  // torn down/rebuilt.
  const bool mapLoaded = MapMgr::GetIsMapLoaded();
  const uint32_t myId = GetMyId();
  Log::Info("AgentMgr: IssueNativeMove begin target=(%.0f, %.0f) map=%u "
            "loaded=%d myId=%u moveFn=0x%p",
            x, y, MapMgr::GetMapId(), mapLoaded ? 1 : 0, myId, s_moveFn);
  if (!mapLoaded || myId == 0) {
    Log::Info("AgentMgr: IssueNativeMove skipped because world is not ready");
    return; // silently skip - caller will retry on next tick
  }

  // Reject obviously invalid coordinates (corrupted agent reads, NaN, etc.)
  // GW map coordinates are typically in [-30000, 30000] range.
  if (x < -50000.0f || x > 50000.0f || y < -50000.0f || y > 50000.0f ||
      x != x || y != y) { // NaN check
    Log::Warn("AgentMgr: IssueNativeMove rejected invalid coords (%.0f, %.0f)",
              x, y);
    return;
  }

  if (ShouldSuppressMove(x, y)) {
    return;
  }

  MoveData moveData{};
  moveData.x = x;
  moveData.y = y;
  moveData.plane = 0;
  InvokeNativeMoveRaw(&moveData);
  s_lastIssuedMove = moveData;
  s_lastIssuedMoveAt = GetTickCount();
  s_haveLastIssuedMove = true;
  Log::Info("AgentMgr: IssueNativeMove returned target=(%.0f, %.0f)", x, y);
}

} // namespace

bool IsCasting(const AgentLiving *agent) { return IsCastingState(agent); }

void Move(float x, float y) {
  if (!s_moveFn) {
    if (!s_loggedMoveOnce) {
      Log::Info("AgentMgr: Move via CtoS (no scanned fn)");
      s_loggedMoveOnce = true;
    }
    CtoS::MoveToCoord(x, y);
    return;
  }
  if (IsCastingState(GetMyAgent())) {
    return;
  }
  if (!GameThread::IsOnGameThread() && CtoS::Initialize() &&
      CtoS::IsBotshubCommandLaneAvailable()) {
    if (!s_loggedEngineMoveLane) {
      Log::Info("AgentMgr: Move using engine command lane");
      s_loggedEngineMoveLane = true;
    }
    MoveCommand cmd{};
    cmd.fn = reinterpret_cast<uintptr_t>(&BotshubMoveCommandStub);
    cmd.move.x = x;
    cmd.move.y = y;
    cmd.move.plane = 0u;
    if (CtoS::EnqueueBotshubCommand(&cmd, sizeof(cmd))) {
      return;
    }
    Log::Warn("AgentMgr: Botshub command queue rejected move, falling back");
  } else if (!GameThread::IsOnGameThread() && CtoS::Initialize() &&
             !s_loggedEngineMoveLaneUnavailable) {
    Log::Info("AgentMgr: Move skipping engine command lane because it is "
              "unavailable");
    s_loggedEngineMoveLaneUnavailable = true;
  }

  if (GameThread::IsInitialized()) {
    // Fallback only. Sparkfly route movement is more stable on the
    // BotsHub-style engine lane; repeated native GameThread post-dispatch
    // moves can stall the render/game-thread hook.
    if (!GameThread::IsOnGameThread()) {
      if (!s_loggedMoveQueuedOnce) {
        Log::Info(
            "AgentMgr: Move queuing native move on GameThread post-dispatch");
        s_loggedMoveQueuedOnce = true;
      }
      AcquireSRWLockExclusive(&s_moveQueueLock);
      s_pendingQueuedMove.x = x;
      s_pendingQueuedMove.y = y;
      s_pendingQueuedMove.plane = 0;
      s_pendingQueuedMoveValid = true;
      ReleaseSRWLockExclusive(&s_moveQueueLock);

      if (InterlockedCompareExchange(&s_moveDrainQueued, 1, 0) == 0) {
        GameThread::EnqueuePost([]() { DrainQueuedMove(); });
      }
      return;
    }

    IssueNativeMove(x, y);
    return;
  }

  Log::Warn(
      "AgentMgr: Move falling back to packet path (GameThread not ready)");
  CtoS::MoveToCoord(x, y);
}

void ResetMoveState(const char *reason) {
  MoveData pendingMove{};
  bool hadPendingMove = false;
  AcquireSRWLockExclusive(&s_moveQueueLock);
  if (s_pendingQueuedMoveValid) {
    pendingMove = s_pendingQueuedMove;
    hadPendingMove = true;
  }
  s_pendingQueuedMove = {};
  s_pendingQueuedMoveValid = false;
  ReleaseSRWLockExclusive(&s_moveQueueLock);

  const LONG hadDrainQueued = InterlockedExchange(&s_moveDrainQueued, 0);
  const bool hadLastIssuedMove = s_haveLastIssuedMove;
  const MoveData lastIssuedMove = s_lastIssuedMove;
  const DWORD lastIssuedAgeMs = hadLastIssuedMove && s_lastIssuedMoveAt != 0u
                                    ? (GetTickCount() - s_lastIssuedMoveAt)
                                    : 0u;
  s_lastIssuedMove = {};
  s_lastIssuedMoveAt = 0u;
  s_haveLastIssuedMove = false;

  Log::Info("AgentMgr: ResetMoveState reason=%s hadPending=%d "
            "pendingTarget=(%.0f, %.0f) hadDrainQueued=%ld hadLast=%d "
            "lastTarget=(%.0f, %.0f) lastAgeMs=%lu",
            reason != nullptr ? reason : "", hadPendingMove ? 1 : 0,
            hadPendingMove ? pendingMove.x : 0.0f,
            hadPendingMove ? pendingMove.y : 0.0f,
            static_cast<long>(hadDrainQueued), hadLastIssuedMove ? 1 : 0,
            hadLastIssuedMove ? lastIssuedMove.x : 0.0f,
            hadLastIssuedMove ? lastIssuedMove.y : 0.0f,
            static_cast<unsigned long>(lastIssuedAgeMs));
}

static bool s_loggedChangeTargetFallback = false;
static uint32_t s_lastQueuedTargetId = 0u;
static DWORD s_lastQueuedTargetAt = 0u;
static constexpr DWORD kChangeTargetMoveSettleMs = 1500u;

static void ChangeTargetImpl(uint32_t agentId, bool respectMoveSettle,
                             const char *modeLabel) {
  const DWORD now = GetTickCount();
  if (GetTargetId() == agentId) {
    s_lastQueuedTargetId = agentId;
    s_lastQueuedTargetAt = now;
    return;
  }
  if (agentId == s_lastQueuedTargetId && (now - s_lastQueuedTargetAt) < 250u) {
    return;
  }
  if (respectMoveSettle && agentId != 0u && s_haveLastIssuedMove &&
      (now - s_lastIssuedMoveAt) < kChangeTargetMoveSettleMs) {
    Log::Info("AgentMgr: ChangeTarget deferred until movement settles");
    s_lastQueuedTargetId = agentId;
    s_lastQueuedTargetAt = now;
    return;
  }

  s_lastQueuedTargetId = agentId;
  s_lastQueuedTargetAt = now;

  if (!GameThread::IsOnGameThread() && CtoS::Initialize() &&
      CtoS::IsBotshubCommandLaneAvailable()) {
    if (!s_loggedChangeTargetEngineLane) {
      Log::Info("AgentMgr: ChangeTarget using engine command lane");
      s_loggedChangeTargetEngineLane = true;
    }
    ChangeTargetCommand cmd{};
    cmd.fn = reinterpret_cast<uintptr_t>(&BotshubChangeTargetCommandStub);
    cmd.agent_id = agentId;
    if (CtoS::EnqueueBotshubCommand(&cmd, sizeof(cmd))) {
      if (!respectMoveSettle) {
        Log::Info("AgentMgr: ChangeTarget forced target=%u via %s", agentId,
                  modeLabel);
      }
      return;
    }
    Log::Warn("AgentMgr: ChangeTarget engine command lane rejected target=%u",
              agentId);
  }

  if (GameThread::IsOnGameThread()) {
    if (!s_loggedChangeTargetNativeLane) {
      Log::Info("AgentMgr: ChangeTarget using native GameThread path");
      s_loggedChangeTargetNativeLane = true;
    }
    if (!respectMoveSettle) {
      Log::Info("AgentMgr: ChangeTarget forced target=%u via %s", agentId,
                modeLabel);
    }
    InvokeChangeTargetRaw(agentId);
    return;
  }
  if (GameThread::IsInitialized()) {
    if (!s_loggedChangeTargetNativeLane) {
      Log::Info(
          "AgentMgr: ChangeTarget using native post-dispatch fallback path");
      s_loggedChangeTargetNativeLane = true;
    }
    GameThread::EnqueuePost([agentId, respectMoveSettle, modeLabel]() {
      if (!respectMoveSettle) {
        Log::Info("AgentMgr: ChangeTarget forced target=%u via %s", agentId,
                  modeLabel);
      }
      InvokeChangeTargetRaw(agentId);
    });
    return;
  }

  if (!s_loggedChangeTargetFallback) {
    Log::Warn("AgentMgr: ChangeTarget falling back to direct raw path");
    s_loggedChangeTargetFallback = true;
  }
  InvokeChangeTargetRaw(agentId);
}

void ChangeTarget(uint32_t agentId) {
  ChangeTargetImpl(agentId, true, "normal");
}

void ForceChangeTarget(uint32_t agentId) {
  ChangeTargetImpl(agentId, false, "force");
}

uint32_t GetTargetId() {
  if (Offsets::CurrentTarget) {
    __try {
      const uint32_t value =
          *reinterpret_cast<uint32_t *>(Offsets::CurrentTarget);
      if (!s_loggedCurrentTargetRead) {
        Log::Info("AgentMgr: CurrentTarget ptr=0x%08X value=%u",
                  static_cast<unsigned>(Offsets::CurrentTarget), value);
        s_loggedCurrentTargetRead = true;
      }
      if (value != 0)
        return value;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      if (!s_loggedCurrentTargetFault) {
        Log::Warn("AgentMgr: CurrentTarget read fault at 0x%08X",
                  static_cast<unsigned>(Offsets::CurrentTarget));
        s_loggedCurrentTargetFault = true;
      }
    }
  }

  return GetTargetIdFromLog();
}

uint32_t GetTargetIdFromLog() {
  const uint32_t myId = GetMyId();
  if (!myId)
    return 0;

  if (!s_loggedTargetLogStats) {
    Log::Info("AgentMgr: TargetLog stats calls=%u stores=%u",
              TargetLogHook::GetCallCount(), TargetLogHook::GetStoreCount());
    s_loggedTargetLogStats = true;
  }

  const uint32_t value = TargetLogHook::GetTarget(myId);
  if (value && !s_loggedTargetLogRead) {
    Log::Info("AgentMgr: TargetLog[MyID=%u] = %u", myId, value);
    s_loggedTargetLogRead = true;
  }
  return value;
}

// Cache for GetMyId — Offsets::MyID transiently reads 0 for a few
// hundred ms after a player skill cast (the game briefly rewrites the
// slot during cast state transitions). Without the cache, every caller
// that gates on "do we have a player" (GetPlayerSkillbar,
// MapMgr::GetIsMapLoaded, MovePlayerNear's position read) sees 0 and
// fails, which cascades into broken skill dispatch, broken movement,
// and map_not_loaded action rejections for minutes.
//
// The cache holds the last non-zero id for up to kMyIdCacheTtlMs. The
// id can only legitimately change on zone transition or disconnect, and
// both of those take far longer than the TTL, so a stale cache is
// bounded and recoverable.
static DWORD s_cachedMyIdTick = 0;
static uint32_t s_cachedMyId = 0;
static constexpr DWORD kMyIdCacheTtlMs = 1500;

uint32_t GetMyId() {
  if (!Offsets::MyID)
    return 0;
  uint32_t liveId = *reinterpret_cast<uint32_t *>(Offsets::MyID);
  const DWORD now = GetTickCount();
  if (liveId != 0) {
    s_cachedMyId = liveId;
    s_cachedMyIdTick = now;
    return liveId;
  }
  // Live read is 0. If the cache is fresh enough, trust it.
  if (s_cachedMyId != 0 && (now - s_cachedMyIdTick) <= kMyIdCacheTtlMs) {
    return s_cachedMyId;
  }
  s_cachedMyId = 0;
  return 0;
}

void Attack(uint32_t agentId) {
  static bool s_loggedActionAttack = false;
  if (!s_loggedActionAttack) {
    s_loggedActionAttack = true;
    Log::Info(
        "AgentMgr: Attack using ACTION_ATTACK packet path with call-target");
  }
  CtoS::ActionAttack(agentId, 1u);
}

void CancelAction() { CtoS::CancelAction(); }

bool ActionInteract() {
  const bool fired = UIMgr::PerformUiAction(kActionInteractCode);
  if (!fired) {
    Log::Warn("AgentMgr: ActionInteract unavailable (PerformUiAction failed)");
  }
  return fired;
}

bool InteractAgentWorldAction(uint32_t agentId, bool callTarget) {
  if (agentId == 0u) {
    return false;
  }

  const uint32_t ct = callTarget ? 1u : 0u;
  if (GameThread::IsInitialized() && Offsets::UIMessage > 0x10000) {
    if (!s_loggedInteractAgentWorldAction) {
      Log::Info(
          "AgentMgr: InteractAgentWorldAction using UI message path msg=0x%08X",
          kSendWorldActionUiMessage);
      s_loggedInteractAgentWorldAction = true;
    }
    GameThread::Enqueue([agentId, ct]() {
      WorldActionUIPacket packet{
          static_cast<uint32_t>(ResolveWorldActionId(agentId)), agentId, ct};
      UIMgr::SendUIMessage(kSendWorldActionUiMessage, &packet, nullptr);
    });
    return true;
  }

  if (s_worldActionFn && GameThread::IsInitialized()) {
    if (!s_loggedInteractAgentWorldAction) {
      Log::Info(
          "AgentMgr: InteractAgentWorldAction using native world-action path "
          "fn=0x%08X",
          static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_worldActionFn)));
      s_loggedInteractAgentWorldAction = true;
    }
    GameThread::Enqueue([agentId, ct]() { InvokeWorldActionRaw(agentId, ct); });
    return true;
  }

  if (!s_loggedInteractAgentWorldActionFallback) {
    Log::Warn("AgentMgr: InteractAgentWorldAction unavailable");
    s_loggedInteractAgentWorldActionFallback = true;
  }
  return false;
}

void CallTarget(uint32_t agentId) {
  // Packet path proven working, matches AutoIt: SendPacket(0xC, CALL_TARGET,
  // 0xA, agentId) Native function path is resolved but the dispatcher+offset
  // may not point to the correct CallTarget sub-function in all GW builds.
  CtoS::SendPacket(3, Packets::CALL_TARGET,
                   static_cast<uint32_t>(CallTargetType::AttackingOrTargetting),
                   agentId);
}

void InteractItem(uint32_t agentId, bool callTarget) {
  if (!s_interactItemFn) {
    CtoS::PickUpItem(agentId);
    return;
  }
  if (!GameThread::IsInitialized()) {
    Log::Warn("AgentMgr: InteractItem falling back to packet path (GameThread "
              "not ready)");
    CtoS::PickUpItem(agentId);
    return;
  }

  auto fn = s_interactItemFn;
  const uint32_t ct = callTarget ? 1u : 0u;
  GameThread::Enqueue([fn, agentId, ct]() { fn(agentId, ct); });
}

void InteractNPC(uint32_t agentId) {
  InteractNPCEx(agentId, NpcInteractMode::NativePostCallTarget);
}

void InteractNPCEx(uint32_t agentId, NpcInteractMode mode) {
  const bool worldActionMode =
      mode == NpcInteractMode::WorldActionNoCallTarget ||
      mode == NpcInteractMode::WorldActionCallTarget;
  if (worldActionMode && GameThread::IsInitialized() &&
      Offsets::UIMessage > 0x10000) {
    const uint32_t ct =
        mode == NpcInteractMode::WorldActionCallTarget ? 1u : 0u;
    if (!s_loggedInteractNpcWorldAction) {
      Log::Info(
          "AgentMgr: InteractNPC using WorldAction UI message path msg=0x%08X",
          kSendWorldActionUiMessage);
      s_loggedInteractNpcWorldAction = true;
    }
    if (!s_loggedInteractNpcVariant) {
      Log::Info("AgentMgr: InteractNPC variant mode=%s msg=0x%08X",
                NpcInteractModeName(mode), kSendWorldActionUiMessage);
      s_loggedInteractNpcVariant = true;
    }
    GameThread::Enqueue([agentId, ct]() {
      WorldActionUIPacket packet{
          static_cast<uint32_t>(ResolveWorldActionId(agentId)), agentId, ct};
      UIMgr::SendUIMessage(kSendWorldActionUiMessage, &packet, nullptr);
    });
    return;
  }

  const bool nativeMode = mode == NpcInteractMode::NativePostCallTarget ||
                          mode == NpcInteractMode::NativePostNoCallTarget ||
                          mode == NpcInteractMode::NativePreCallTarget ||
                          mode == NpcInteractMode::NativePreNoCallTarget;
  if (nativeMode && s_interactNpcFn && GameThread::IsInitialized()) {
    const uint32_t ct = (mode == NpcInteractMode::NativePostCallTarget ||
                         mode == NpcInteractMode::NativePreCallTarget)
                            ? 1u
                            : 0u;
    if (!s_loggedInteractNpcNative) {
      Log::Info(
          "AgentMgr: InteractNPC using native GameThread path fn=0x%08X "
          "callTarget=1",
          static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_interactNpcFn)));
      s_loggedInteractNpcNative = true;
    }
    if (!s_loggedInteractNpcVariant &&
        mode != NpcInteractMode::NativePostCallTarget) {
      Log::Info(
          "AgentMgr: InteractNPC variant mode=%s fn=0x%08X",
          NpcInteractModeName(mode),
          static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_interactNpcFn)));
      s_loggedInteractNpcVariant = true;
    }
    auto fn = s_interactNpcFn;
    if (mode == NpcInteractMode::NativePreCallTarget ||
        mode == NpcInteractMode::NativePreNoCallTarget) {
      GameThread::Enqueue([fn, agentId, ct]() { fn(agentId, ct); });
    } else {
      GameThread::EnqueuePost([fn, agentId, ct]() { fn(agentId, ct); });
    }
    return;
  }

  if (!s_loggedInteractNpcFallback) {
    Log::Warn("AgentMgr: InteractNPC falling back to raw packet path");
    s_loggedInteractNpcFallback = true;
  }
  if (!s_loggedInteractNpcVariant &&
      mode != NpcInteractMode::NativePostCallTarget) {
    Log::Info("AgentMgr: InteractNPC packet variant mode=%s",
              NpcInteractModeName(mode));
    s_loggedInteractNpcVariant = true;
  }

  if (mode == NpcInteractMode::PacketNpc12) {
    CtoS::SendPacket(3, Packets::INTERACT_NPC, agentId, 0u);
    return;
  }
  CtoS::SendPacket(3, Packets::INTERACT_NPC, agentId, 0u);
}

void InteractPlayer(uint32_t agentId) {
  CtoS::SendPacket(2, Packets::INTERACT_PLAYER, agentId);
}

void InteractSignpost(uint32_t agentId) {
  if (agentId == 0u) {
    return;
  }
  CtoS::SendPacket(3, Packets::SIGNPOST_RUN, agentId, 0u);
}

void InteractSignpostLegacy(uint32_t agentId) {
  if (agentId == 0u) {
    return;
  }
  if (!s_loggedLegacySignpostPath) {
    Log::Info("AgentMgr: InteractSignpostLegacy using direct INTERACT_GADGET "
              "packet path");
    s_loggedLegacySignpostPath = true;
  }
  CtoS::SendPacketDirect(3, Packets::INTERACT_GADGET, agentId, 0u);
}

// Agent access via flat pointer chain: *AgentBase = agent_ptr_array,
// *(AgentBase+8) = maxAgents
Agent *GetAgentByID(uint32_t agentId) {
  if (!Offsets::AgentBase || agentId == 0)
    return nullptr;
  __try {
    uintptr_t agentArr = *reinterpret_cast<uintptr_t *>(Offsets::AgentBase);
    if (agentArr <= 0x10000)
      return nullptr;
    uint32_t maxAgents =
        *reinterpret_cast<uint32_t *>(Offsets::AgentBase + 0x8);
    if (agentId >= maxAgents)
      return nullptr;
    uintptr_t agentPtr = *reinterpret_cast<uintptr_t *>(agentArr + agentId * 4);
    if (agentPtr <= 0x10000)
      return nullptr;
    return reinterpret_cast<Agent *>(agentPtr);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

AgentLiving *GetMyAgent() {
  uint32_t id = GetMyId();
  if (!id)
    return nullptr;
  Agent *agent = GetAgentByID(id);
  if (!agent)
    return nullptr;
  // Check type == 0xDB (Living), but also accept if type field is valid
  if (agent->type == 0xDB)
    return static_cast<AgentLiving *>(agent);
  // In GW Reforged the type field may differ — check at struct level
  return nullptr;
}

AgentLiving *GetTargetAsLiving() {
  uint32_t id = GetTargetId();
  if (!id)
    return nullptr;
  Agent *agent = GetAgentByID(id);
  if (!agent || agent->type != 0xDB)
    return nullptr;
  return static_cast<AgentLiving *>(agent);
}

uint32_t GetMaxAgents() {
  if (!Offsets::AgentBase)
    return 0;
  __try {
    return *reinterpret_cast<uint32_t *>(Offsets::AgentBase + 0x8);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}

float GetSquaredDistance(float x1, float y1, float x2, float y2) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  return dx * dx + dy * dy;
}

float GetDistance(float x1, float y1, float x2, float y2) {
  return sqrtf(GetSquaredDistance(x1, y1, x2, y2));
}

bool GetAgentExists(uint32_t agentId) {
  return GetAgentByID(agentId) != nullptr;
}

// --- Encoded agent name resolution ---
//
// Mirrors GWCA AgentMgr::GetAgentEncName. WorldContext layout offsets
// from GWCA's WorldContext.h (verified working in this build since
// PlayerMgr already uses +0x80C successfully for the PlayerArray):
//   +0x7CC  AgentInfoArray  agent_infos   (GWArray<AgentInfo>, 16 bytes)
//   +0x7FC  NPCArray        npcs          (GWArray<NPC>,       16 bytes)
//   +0x80C  PlayerArray     players       (GWArray<Player>,    16 bytes)
//
// AgentInfo has name_enc at +0x34 (sizeof 0x38). NPC has name_enc at
// +0x20 (sizeof 0x30). Player has name_enc at +0x24.
//
// For LIVING agents:
//   - If ag->login_number != 0 -> it's a human player, read from
//   - If ag->login_number != 0 → it's a human player, read from
//     players[login_number].name_enc.
//   - Otherwise try agent_infos[ag->agent_id].name_enc first; fall
//     back to npcs[ag->player_number].name_enc (dummy agents like
//     "Suit of xx Armor" only live in NPCArray). See GWCA comments
//     around AgentMgr.cpp:440-449 for the historical rationale.
//
// For ITEM agents and GADGET agents the lookup differs (ItemMgr or
// GadgetInfo); those are not implemented here yet. This helper only
// covers living agents, which is what BuildNearbyAgentsJson needs.
static constexpr uintptr_t kAgentInfosOffset = 0x7CC;
static constexpr uintptr_t kNpcsOffset = 0x7FC;
static constexpr uintptr_t kPlayersOffset = 0x80C;
static constexpr size_t kAgentInfoSize = 0x38;
static constexpr size_t kAgentInfoNameEnc = 0x34;

template <typename T>
static GWArray<T> *SafeReadArray(uintptr_t worldCtx, uintptr_t fieldOffset,
                                 uint32_t maxSize) {
  __try {
    auto *arr = reinterpret_cast<GWArray<T> *>(worldCtx + fieldOffset);
    if (!arr->buffer || arr->size == 0 || arr->size > maxSize) {
      return nullptr;
    }
    return arr;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

static wchar_t *ReadAgentInfoNameEnc(uintptr_t worldCtx, uint32_t agentId) {
  __try {
    auto *arr =
        reinterpret_cast<GWArray<uint8_t> *>(worldCtx + kAgentInfosOffset);
    if (!arr->buffer || arr->size == 0 || arr->size > 0x4000)
      return nullptr;
    if (agentId >= arr->size)
      return nullptr;
    uintptr_t slot = reinterpret_cast<uintptr_t>(arr->buffer) +
                     static_cast<uintptr_t>(agentId) * kAgentInfoSize;
    return *reinterpret_cast<wchar_t **>(slot + kAgentInfoNameEnc);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

// --- Gadget name resolution ---
//
// Gadget names live outside WorldContext. GWCA's algorithm
// (AgentMgr.cpp:451-465):
//   1. GameContext.agent (AgentContext*, at GC+0x08)
//   2. AgentContext.agent_summary_info[agent_id].extra_info_sub
//      (AgentSummaryInfo is 12 bytes, extra_info_sub at +0x08;
//       array starts at AgentContext+0x98)
//   3. Prefer extra_info_sub->gadget_name_enc (at +0x10)
//   4. Fall back to GameContext.gadget (GadgetContext*, at GC+0x38)
//      -> GadgetInfo[gadget_id].name_enc (GadgetInfo is 16 bytes,
//         name_enc at +0x0C; array starts at GadgetContext+0x00)
//   5. extra_info_sub->gadget_id lives at +0x08 of the sub struct.
static constexpr uintptr_t kGameCtxAgentContextPtr = 0x08;
static constexpr uintptr_t kGameCtxGadgetContextPtr = 0x38;
static constexpr uintptr_t kAgentSummaryInfoArray = 0x98;
static constexpr size_t kAgentSummaryInfoSize = 0x0C;
static constexpr uintptr_t kAgentSummaryInfoSubPtr = 0x08;
static constexpr uintptr_t kSubStructGadgetId = 0x08;
static constexpr uintptr_t kSubStructGadgetNameEnc = 0x10;
static constexpr size_t kGadgetInfoSize = 0x10;
static constexpr uintptr_t kGadgetInfoNameEnc = 0x0C;

static wchar_t *ReadGadgetNameEnc(uint32_t agentId) {
  const uintptr_t gc = Offsets::ResolveGameContext();
  if (!gc)
    return nullptr;

  __try {
    const uintptr_t agentCtx =
        *reinterpret_cast<uintptr_t *>(gc + kGameCtxAgentContextPtr);
    if (agentCtx <= 0x10000)
      return nullptr;

    auto *asi =
        reinterpret_cast<GWArray<uint8_t> *>(agentCtx + kAgentSummaryInfoArray);
    if (!asi->buffer || asi->size == 0 || asi->size > 0x4000)
      return nullptr;
    if (agentId >= asi->size)
      return nullptr;

    const uintptr_t slot =
        reinterpret_cast<uintptr_t>(asi->buffer) +
        static_cast<uintptr_t>(agentId) * kAgentSummaryInfoSize;
    const uintptr_t subPtr =
        *reinterpret_cast<uintptr_t *>(slot + kAgentSummaryInfoSubPtr);
    if (subPtr <= 0x10000)
      return nullptr;

    // Primary: agent_summary_info[agent_id].extra_info_sub->gadget_name_enc
    if (wchar_t *enc =
            *reinterpret_cast<wchar_t **>(subPtr + kSubStructGadgetNameEnc)) {
      return enc;
    }

    // Fallback: GadgetContext.GadgetInfo[gadget_id].name_enc
    const uint32_t gadgetId =
        *reinterpret_cast<uint32_t *>(subPtr + kSubStructGadgetId);
    const uintptr_t gadgetCtx =
        *reinterpret_cast<uintptr_t *>(gc + kGameCtxGadgetContextPtr);
    if (gadgetCtx <= 0x10000)
      return nullptr;

    auto *gi = reinterpret_cast<GWArray<uint8_t> *>(gadgetCtx);
    if (!gi->buffer || gi->size == 0 || gi->size > 0x4000)
      return nullptr;
    if (gadgetId >= gi->size)
      return nullptr;

    const uintptr_t giSlot = reinterpret_cast<uintptr_t>(gi->buffer) +
                             static_cast<uintptr_t>(gadgetId) * kGadgetInfoSize;
    return *reinterpret_cast<wchar_t **>(giSlot + kGadgetInfoNameEnc);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

wchar_t *GetAgentEncName(const Agent *agent) {
  if (!agent)
    return nullptr;
  const uintptr_t worldCtx = Offsets::ResolveWorldContext();
  if (!worldCtx)
    return nullptr;

  uint32_t agentType = 0;
  uint32_t agentId = 0;
  uint32_t loginNumber = 0;
  uint32_t playerNumber = 0;
  __try {
    agentType = agent->type;
    agentId = agent->agent_id;
    if (agentType == 0xDB) {
      auto *ag = reinterpret_cast<const AgentLiving *>(agent);
      loginNumber = ag->login_number;
      playerNumber = ag->player_number;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }

  // Gadgets (chests, signposts, portals, shrines): resolve via
  // AgentContext.agent_summary_info[agent_id].extra_info_sub with
  // GadgetContext.GadgetInfo[gadget_id] as the fallback.
  if (agentType == 0x200) {
    return ReadGadgetNameEnc(agentId);
  }

  // Item agents are not handled here — BuildNearbyAgentsJson already
  // resolves them via ItemMgr::GetItemById(item.item_id)->name_enc.
  if (agentType != 0xDB)
    return nullptr;

  // Player: look up by login_number in PlayerArray.
  if (loginNumber != 0) {
    auto *players = SafeReadArray<Player>(worldCtx, kPlayersOffset, 1024);
    if (players && loginNumber < players->size) {
      __try {
        return players->buffer[loginNumber].name_enc;
      } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
      }
    }
    return nullptr;
  }

  // NPC: try agent_infos[agent_id].name_enc first...
  if (wchar_t *enc = ReadAgentInfoNameEnc(worldCtx, agentId)) {
    return enc;
  }

  // ...fall back to npcs[player_number].name_enc.
  auto *npcs = SafeReadArray<NPC>(worldCtx, kNpcsOffset, 0x8000);
  if (npcs && playerNumber < npcs->size) {
    __try {
      return npcs->buffer[playerNumber].name_enc;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      return nullptr;
    }
  }
  return nullptr;
}

wchar_t *GetAgentEncName(uint32_t agentId) {
  Agent *agent = GetAgentByID(agentId);
  if (agent) {
    return GetAgentEncName(agent);
  }
  // Agent no longer in the agent array (despawned, etc.) — still try
  // a direct agent_infos lookup in case the slot persists.
  const uintptr_t worldCtx = Offsets::ResolveWorldContext();
  if (!worldCtx)
    return nullptr;
  return ReadAgentInfoNameEnc(worldCtx, agentId);
}

// Plain-name fallback for players: the Player struct carries both an
// encoded reference (+0x24, title-decorated) and a plain wchar handle
// (+0x28, bare account name). When the encoded form hasn't decoded
// yet (player has an active title so name_enc is a format-string ref
// the decoder needs to expand), this returns the plain handle so the
// caller can emit SOMETHING readable immediately.
wchar_t *GetAgentPlainName(const Agent *agent) {
  if (!agent)
    return nullptr;
  const uintptr_t worldCtx = Offsets::ResolveWorldContext();
  if (!worldCtx)
    return nullptr;

  uint32_t agentType = 0;
  uint32_t loginNumber = 0;
  __try {
    agentType = agent->type;
    if (agentType == 0xDB) {
      loginNumber = reinterpret_cast<const AgentLiving *>(agent)->login_number;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }

  if (agentType != 0xDB || loginNumber == 0)
    return nullptr;

  auto *players = SafeReadArray<Player>(worldCtx, kPlayersOffset, 1024);
  if (!players || loginNumber >= players->size)
    return nullptr;
  __try {
    return players->buffer[loginNumber].name;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

} // namespace GWA3::AgentMgr
