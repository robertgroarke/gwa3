# Movement & Skill Dispatch Comparison

How four different GW automation frameworks dispatch Move, UseSkill, and ChangeTarget.

## 1. Old GWA Censured AutoIt (pre-BotsHub migration)

Source: `GWA Censured/lib/GWA2.au3`, `GWA Censured/lib/GWA2_Assembly.au3`

### Architecture

All game actions route through a **single in-process command queue** drained by an inline hook at the `Engine` site. The queue lives in a `VirtualAlloc`'d block inside Gw.exe. AutoIt writes commands from its own process via `WriteProcessMemory`.

### Move

```autoit
Func Move($X, $Y, $random = 50)
    DllStructSetData($MOVE_STRUCT, 2, $X + Random(-$random, $random))
    DllStructSetData($MOVE_STRUCT, 3, $Y + Random(-$random, $random))
    Enqueue($MOVE_STRUCT_PTR, 16)
EndFunc
```

- Writes a 16-byte command blob: `[fn_ptr | float x | float y | dword 0]`
- `fn_ptr` points to `CommandMove` shellcode in the VirtualAlloc block
- `CommandMove` does: `lea eax,[eax+4]; push eax; call Move; pop eax; ljmp CommandReturn`
- Native `Move` signature: `void __cdecl Move(GamePos* pos)` where GamePos = `{float x, float y, uint32_t zplane}`

### UseSkill

```autoit
Func UseSkill($skillSlot, $target, $callTarget)
    DllStructSetData($USE_SKILL_STRUCT, 2, $myID)
    DllStructSetData($USE_SKILL_STRUCT, 3, $skillSlot - 1)
    DllStructSetData($USE_SKILL_STRUCT, 4, $targetID)
    DllStructSetData($USE_SKILL_STRUCT, 5, $callTarget)
    Enqueue($USE_SKILL_STRUCT_PTR, 20)
EndFunc
```

- 20-byte blob: `[fn_ptr | myID | slot-1 | targetID | callTarget]`
- `CommandUseSkill` pushes all 4 params and calls native `UseSkill`
- Native signature: `void __cdecl UseSkill(uint32_t myId, uint32_t zeroSlot, uint32_t targetId, uint32_t callTarget)`

### ChangeTarget

```autoit
Func ChangeTarget($agent)
    DllStructSetData($CHANGE_TARGET_STRUCT, 2, DllStructGetData($agent, 'ID'))
    Enqueue($CHANGE_TARGET_STRUCT_PTR, 8)
EndFunc
```

- 8-byte blob: `[fn_ptr | agentID]`
- `CommandChangeTarget` does: `xor edx,edx; push edx; push [eax+4]; call ChangeTarget; add esp,8`
- Native signature: `void __cdecl ChangeTarget(uint32_t agentId, uint32_t unk=0)`

### Enqueue

```autoit
Func Enqueue($ptr, $size)
    WriteProcessMemory(GetProcessHandle(), 256 * $queue_counter + $queue_base_address, $ptr, $size)
    $queue_counter = Mod($queue_counter + 1, $queue_size)
EndFunc
```

- Circular buffer: 64 slots × 256 bytes each
- `$queue_counter` is the **write head** (AutoIt side)
- `QueueCounter` in the asm data block is the **read tail** (engine side)
- One command drains per engine tick

### Engine Hook (MainProc)

Hooked at `Offsets::Engine` via 5-byte JMP overwrite. The detour:

1. `pushad; pushfd`
2. **Environment gate**: traverses `BasePointer → [0] → [+0x18] → [+0x44]`, checks `[+0x19C]` (state_guard) and `[+0x198]` (env_index), inspects `Environment + index * 0x7C + 0x10` (flags & 0x40001)
3. **HandleCase** (defer): if gate says unsafe, clear and advance the queue counter without executing
4. **RegularFlow** (execute): save index, clear slot, JMP to command stub
5. Command stub calls native function, then `ljmp CommandReturn`
6. `CommandReturn`: check/advance QueueCounter, then `popfd; popad; mov ebp,esp; fld [ebp+8]; ljmp hookAddr+5`

**Key detail**: the exit path (`mov ebp,esp; fld [ebp+8]`) is **hardcoded** in the MainProc assembly, not in a separate trampoline. The `ljmp` is a far jump.

### Packet Sending

```autoit
Func SendPacket($size, $header, $param1, ...)
    DllStructSetData($PACKET_STRUCT, ...)
    Enqueue($PACKET_STRUCT_PTR, 52)
EndFunc
```

Packets also go through the same command queue. `CommandSendPacket` calls the native `PacketSend` from within the engine hook context.

### Threading Model

**Single-threaded.** AutoIt runs in its own process, writes to shared memory. The command queue is drained by the game's own thread at the engine hook site. No background threads, no concurrent PacketSend calls.

---

## 2. Upstream BotsHub

Source: `BotsHub-latest/lib/GWA2.au3`, `BotsHub-latest/lib/GWA2_Assembly.au3`

**Identical architecture** to old GWA Censured. Same command queue, same MainProc hook, same Enqueue function, same command stubs. BotsHub forked from the same GWA2 codebase.

The only differences are in the bot logic layer (farming routines), not in the dispatch infrastructure.

### Bot-Level Timing

The critical difference from our C++ port: upstream AutoIt bots **loop and sleep** between Move and combat:

```autoit
CommandMove(x, y)
While DistanceTo(x, y) > threshold
    Sleep(250)
WEnd
ChangeTarget(enemy)
FightTarget(enemy)
```

Move, ChangeTarget, and UseSkill are **never in the queue simultaneously**. Each command completes (drains from the queue and the game processes it) before the next is issued. This avoids the movement+combat state conflicts we've been hitting.

---

## 3. GWCA / GWToolbox (C++)

Source: `GWA Censured/GWCA-master/Source/AgentMgr.cpp`, `GWA Censured/GWCA-master/Source/GameThreadMgr.cpp`, `GWA Censured/GWCA-master/Source/CtoSMgr.cpp`

### Architecture

**No engine hook. No command queue.** GWCA calls native functions directly via scanned function pointers. Thread safety is handled by the `GameThread` hook, which hooks a function in the render pipeline (`frapi.cpp`).

### GameThread Hook

```cpp
// Hooks a function found via assertion: "p:\code\engine\frame\frapi.cpp", "!s_bufferBits"
void __cdecl OnLeaveGameThread(void* unk) {
    CallFunctions();           // drain queued callbacks
    LeaveGameThread_Ret(unk);  // call original function
}
```

- Uses `GW::HookBase::CreateHook` (MinHook-style proper detour)
- Callbacks fire at a specific point in the render pipeline
- `GameThread::Enqueue()` adds a one-shot callback; if already on the game thread, executes immediately

### Move

```cpp
bool Move(GamePos pos) {
    if (!Move_Func) return false;
    Move_Func(&pos);  // Direct native call, no queue
    return true;
}
```

- **Direct function pointer call** from whatever thread the caller is on
- No thread safety wrapper
- No state guards
- GWToolbox calls this from UI event handlers (click on minimap, etc.)

### ChangeTarget

```cpp
bool ChangeTarget(AgentID agent_id) {
    if (!ChangeTarget_Func) return false;
    ChangeTarget_Func(agent_id, 0);
    return true;
}
```

- **Direct native call**, no threading wrapper
- GWToolbox's `SafeChangeTarget` wraps this in `GameThread::Enqueue`:
  ```cpp
  void SafeChangeTarget(uint32_t agent_id) {
      GW::GameThread::Enqueue([agent_id] {
          GW::Agents::ChangeTarget(GW::Agents::GetAgentByID(agent_id));
      });
  }
  ```

### Packet Sending (CRITICAL)

```cpp
bool CtoS::SendPacket(uint32_t size, void *buffer) {
    if (GameThread::IsInGameThread() || Render::GetIsInRenderLoop()) {
        // Already on game thread — call directly
        SendPacket_Func(*(uint32_t*)game_srv_object_addr, size, buffer);
        return true;
    }
    // NOT on game thread — copy packet and enqueue
    void* buffer_cpy = malloc(size);
    memcpy(buffer_cpy, buffer, size);
    GameThread::Enqueue([buffer_cpy, size]() {
        SendPacket_Func(..., size, buffer_cpy);
        free(buffer_cpy);
    });
    return true;
}
```

**GWCA never calls PacketSend from a background thread.** If not on the game thread, it copies the packet and enqueues it for the game thread to dispatch. This is the critical difference from our sender-thread approach.

### Threading Model

- All native calls happen on the game thread (either directly or via `GameThread::Enqueue`)
- The game thread hook fires at a well-defined point in the render pipeline
- No background sender threads, no engine hook, no command queue
- `CriticalSection` mutex protects the callback queue

---

## 4. Py4GW (Python)

Source: research from `Py4GW_GWCA_Botting_Research.md`

### Architecture

Python bot framework that wraps GWCA-style native calls through a compiled C++ extension (`PyPlayer`, `PySkillbar`, etc.). Uses an **ActionQueue** system with type-specific throttling.

### Move

```python
@staticmethod
def Move(x, y, zPlane=0):
    ActionQueueManager().AddAction("ACTION", PlayerMethods.Move, x, y, zPlane)
```

Low-level:
```python
def Move(x, y, zPlane=0):
    def _action():
        args = (ctypes.c_float * 4)()
        args[0] = x; args[1] = y; args[2] = float(zPlane); args[3] = 0.0
        MoveTo_Func.directCall(args)
    Game.enqueue(_action)
```

- **ActionQueue** with 50ms throttle for `ACTION` type
- `Game.enqueue()` queues to the game thread (same as GWCA's `GameThread::Enqueue`)
- Native function called from game thread context

### UseSkill

```python
def UseSkill(skill_slot, target_agent_id=0):
    skillbar_instance = PySkillbar.Skillbar()
    skillbar_instance.UseSkill(skill_slot, target_agent_id)
```

- Goes through compiled `PySkillbar` C++ module
- Gated by BehaviorTree conditions: IsExplorable, ValidSlot, EnoughEnergy, SkillReady
- Dispatched via action queue with aftercast delay tracking

### ChangeTarget

```python
def ChangeTarget(agent_id):
    def _action():
        UIManager.SendUIMessage(UIMessage.kSendChangeTarget, [target.agent_id])
    Game.enqueue(_action)
```

**Uses UIMessage, NOT the native ChangeTarget function.** This is the safest approach — it goes through the game's own UI message system, which handles all state checks internally.

### Threading Model

- All native calls go through `Game.enqueue()` → game thread
- ActionQueue adds throttling (50ms-1250ms depending on action type)
- No background threads calling native functions
- No engine hook

---

## Summary

| Aspect | GWA2/BotsHub | GWCA/GWToolbox | Py4GW | Our gwa3 |
|---|---|---|---|---|
| **Move dispatch** | Engine hook command queue | Direct native call | Game thread via ActionQueue | Engine hook command queue |
| **UseSkill dispatch** | Engine hook command queue | Direct native call | Game thread via PySkillbar | ??? (all paths crash) |
| **ChangeTarget dispatch** | Engine hook command queue | Direct native call (SafeChangeTarget via GameThread) | **UIMessage** via game thread | GameThread::EnqueuePost |
| **PacketSend dispatch** | Engine hook command queue | **Game thread only** (enqueue if off-thread) | Game thread | **Background sender thread** (UNSAFE) |
| **Engine hook** | Yes (MainProc at Engine site) | **No** | **No** | Yes (EngineDetourNaked) |
| **Background threads** | None | None | None | Sender thread + watchdog |
| **Command serialization** | One per engine tick | Immediate | 50ms throttle | One per engine tick |
| **State gating** | Environment flags in MainProc | None | BehaviorTree conditions | Movement guard |

## Critical Differences From Our Implementation

### 1. PacketSend thread safety
GWCA: always on game thread. Py4GW: always on game thread. Us: background sender thread. **Our sender thread is the proven crash cause for USE_SKILL packets.**

### 2. Engine hook exit path
GWA2/BotsHub: hardcoded `mov ebp,esp; fld [ebp+8]; ljmp` in MainProc assembly. Us: separate VirtualAlloc'd trampoline with near JMP. The trampoline approach may be corrupting hero rendering.

### 3. Command timing
GWA2/BotsHub: bot loops with Sleep, never queues Move+ChangeTarget+UseSkill simultaneously. Us: AggroMoveToEx fires all three in rapid succession during combat movement.

### 4. ChangeTarget mechanism
Py4GW: uses `UIMessage.kSendChangeTarget` — no native function call at all. GWCA: direct native but GWToolbox wraps in GameThread. Us: GameThread::EnqueuePost with movement guard.

## Recommended Changes

1. **Kill the sender thread.** Route all PacketSend through GameThread::Enqueue (GWCA pattern). This fixes the USE_SKILL crash.
2. **Try UIMessage for ChangeTarget** instead of the native function (Py4GW pattern). This avoids the deadlock/crash from native ChangeTarget during movement.
3. **Investigate the trampoline** vs hardcoded exit path for the engine hook. The hero floating may be from the near-JMP trampoline vs upstream's far-JMP hardcoded exit.
4. **Add bot-level serialization** — don't issue ChangeTarget/UseSkill until the current Move has drained from the engine queue and the character has stopped.
