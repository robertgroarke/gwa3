#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Log.h>

#include <cstring>

namespace GWA3::Offsets {

// ===== Storage =====
uintptr_t BasePointer = 0;
uintptr_t Ping = 0;
uintptr_t StatusCode = 0;
uintptr_t PacketSend = 0;
uintptr_t PacketLocation = 0;
uintptr_t Action = 0;
uintptr_t ActionBase = 0;
uintptr_t Environment = 0;
uintptr_t PreGame = 0;
uintptr_t FrameArray = 0;
uintptr_t SceneContext = 0;

uintptr_t SkillBase = 0;
uintptr_t SkillTimer = 0;
uintptr_t UseSkill = 0;
uintptr_t UseHeroSkill = 0;

uintptr_t FriendList = 0;
uintptr_t PlayerStatus = 0;
uintptr_t AddFriend = 0;
uintptr_t RemoveFriend = 0;

uintptr_t AttributeInfo = 0;
uintptr_t IncreaseAttribute = 0;
uintptr_t DecreaseAttribute = 0;

uintptr_t Transaction = 0;
uintptr_t BuyItemBase = 0;
uintptr_t RequestQuote = 0;
uintptr_t Salvage = 0;
uintptr_t SalvageGlobal = 0;

uintptr_t AgentBase = 0;
uintptr_t ChangeTarget = 0;
uintptr_t CurrentTarget = 0;
uintptr_t TargetLog = 0;
uintptr_t MyID = 0;

uintptr_t Move = 0;
uintptr_t ClickCoords = 0;
uintptr_t InstanceInfo = 0;
uintptr_t WorldConst = 0;
uintptr_t Region = 0;
uintptr_t AreaInfo = 0;

uintptr_t TradeCancel = 0;

uintptr_t UIMessage = 0;
uintptr_t CompassFlag = 0;
uintptr_t PartySearchButtonCallback = 0;
uintptr_t PartyWindowButtonCallback = 0;
uintptr_t EnterMission = 0;
uintptr_t SetDifficulty = 0;
uintptr_t OpenChest = 0;
uintptr_t Dialog = 0;
uintptr_t AiMode = 0;
uintptr_t HeroCommand = 0;
uintptr_t HeroSkills = 0;

uintptr_t PlayerAdd = 0;
uintptr_t PlayerKick = 0;
uintptr_t PartyInvitations = 0;

uintptr_t ActiveQuest = 0;

uintptr_t Engine = 0;
uintptr_t Render = 0;
uintptr_t LoadFinished = 0;
uintptr_t Trader = 0;
uintptr_t TradePartner = 0;

uintptr_t ValidateAsyncDecodeStr = 0;

uintptr_t PostMessage = 0;
uintptr_t ChatLog = 0;

uintptr_t TitleClientDataBase = 0;

uintptr_t CameraClass = 0;
uintptr_t FogPatch = 0;

uintptr_t SendFrameUIMsg = 0;

// ===== Pattern table =====

struct PatternDef {
    const char* name;
    uintptr_t*  target;       // pointer to the storage variable
    const char*  bytes;        // hex pattern (nullptr for assertion-based)
    const char*  mask;         // mask string ('x' = match, '?' = wildcard)
    int          offset;       // offset added to scan result
    Priority     priority;
    PatternType  type;
    // For assertion-based patterns:
    const char*  assertFile;   // source file path (nullptr if hex-pattern)
    const char*  assertMsg;    // assertion message (nullptr if hex-pattern)
};

// Helper to define a hex-pattern entry
#define PAT(name_, target_, bytes_, mask_, off_, pri_, typ_) \
    { name_, &target_, bytes_, mask_, off_, pri_, typ_, nullptr, nullptr }

// Helper to define an assertion-pattern entry
#define ASSERT_PAT(name_, target_, off_, pri_, typ_, file_, msg_) \
    { name_, &target_, nullptr, nullptr, off_, pri_, typ_, file_, msg_ }

// NOTE: AutoIt ASM scanner offsets use formula: result = match_last_byte - size + offset
// which equals match_start - 1 + offset. Our C++ scanner uses match_start + offset.
// Therefore: cpp_offset = autoit_offset - 1
// Assertion patterns and SendFrameUIMsg (custom) don't need the -1 adjustment.

static const PatternDef s_patterns[] = {
    // ===== Core (P0) =====
    PAT("BasePointer",    BasePointer,    "\x50\x6A\x0F\x6A\x00\xFF\x35",       "xxxxxxx",  0x7,    Priority::P0, PatternType::Ptr),
    PAT("PacketSend",     PacketSend,     "\xC7\x47\x54\x00\x00\x00\x00\x81\xE6", "xxxxxxxxx", -0x50, Priority::P0, PatternType::Func),
    PAT("PacketLocation", PacketLocation, "\x83\xC4\x04\x33\xC0\x8B\xE5\x5D\xC3\xA1", "xxxxxxxxxx", 0xA, Priority::P0, PatternType::Ptr),
    PAT("AgentBase",      AgentBase,      "\x8B\x0C\x90\x85\xC9\x74\x19",       "xxxxxxx",  -0x4,   Priority::P0, PatternType::Ptr),
    PAT("MyID",           MyID,           "\x83\xEC\x08\x56\x8B\xF1\x3B\x15",   "xxxxxxxx", -0x4,   Priority::P0, PatternType::Ptr),

    // ===== Core (P1) =====
    PAT("Ping",           Ping,           "\x56\x8B\x75\x08\x89\x16\x5E",       "xxxxxxx",  -0x4,   Priority::P1, PatternType::Ptr),
    PAT("StatusCode",     StatusCode,     "\x89\x45\x08\x8D\x45\x08\x6A\x04",   "xxxxxxxx", -0x11,  Priority::P1, PatternType::Ptr),
    PAT("Action",         Action,         "\x8B\x75\x08\x57\x8B\xF9\x83\xFE\x09\x75\x0C\x68\x77", "xxxxxxxxxxxxx", -0x4, Priority::P1, PatternType::Func),
    PAT("ActionBase",     ActionBase,     "\x8D\x1C\x87\x89\x9D\xF4",           "xxxxxx",   -0x4,   Priority::P1, PatternType::Ptr),
    PAT("Environment",    Environment,    "\x6B\xC6\x7C\x5E\x05",               "xxxxx",    0x5,    Priority::P1, PatternType::Ptr),
    PAT("SceneContext",   SceneContext,   "\xD9\xE0\xD9\x5D\xFC\x8B\x01",       "xxxxxxx",  -0x1,   Priority::P1, PatternType::Ptr),
    ASSERT_PAT("PreGame",    PreGame,    0,  Priority::P1, PatternType::Ptr,  "P:\\Code\\Gw\\Ui\\UiPregame.cpp",          "!s_scene"),
    ASSERT_PAT("FrameArray", FrameArray, 0,  Priority::P0, PatternType::Ptr,  "P:\\Code\\Engine\\Frame\\FrMsg.cpp",       "frame"),

    // ===== Skills (P0) =====
    PAT("SkillBase",      SkillBase,      "\x69\xC6\xA4\x00\x00\x00\x5E",       "xxxxxxx",  0x8,    Priority::P0, PatternType::Ptr),
    PAT("SkillTimer",     SkillTimer,     "\xFF\xD6\x8B\x4D\xF0\x8B\xD8\x8B\x47\x08", "xxxxxxxxxx", -0x4, Priority::P0, PatternType::Ptr),
    PAT("UseSkill",       UseSkill,       "\x85\xF6\x74\x5B\x83\xFE\x11\x74",   "xxxxxxxx", -0x128, Priority::P0, PatternType::Func),
    PAT("UseHeroSkill",   UseHeroSkill,   "\xBA\x02\x00\x00\x00\xB9\x54\x08\x00\x00", "xxxxxxxxxx", -0x5A, Priority::P0, PatternType::Func),

    // ===== Friends (P2) =====
    ASSERT_PAT("FriendList",  FriendList,  0, Priority::P2, PatternType::Ptr, "P:\\Code\\Gw\\Friend\\FriendApi.cpp", "friendName && *friendName"),
    PAT("PlayerStatus",   PlayerStatus,   "\x83\xFE\x03\x77\x40\xFF\x24\xB5\x00\x00\x00\x00\x33\xC0", "xxxxxxxxxxxxxx", -0x26, Priority::P2, PatternType::Func),
    PAT("AddFriend",      AddFriend,      "\x8B\x75\x10\x83\xFE\x03\x74\x65",   "xxxxxxxx", -0x48,  Priority::P2, PatternType::Func),
    PAT("RemoveFriend",   RemoveFriend,   "\x83\xF8\x03\x74\x1D\x83\xF8\x04\x74\x18", "xxxxxxxxxx", -0x1, Priority::P2, PatternType::Func),

    // ===== Attributes (P1) =====
    PAT("AttributeInfo",     AttributeInfo,     "\xBA\x33\x00\x00\x00\x89\x08\x8D\x40\x04", "xxxxxxxxxx", -0x4, Priority::P1, PatternType::Ptr),
    PAT("IncreaseAttribute", IncreaseAttribute, "\x8B\x7D\x08\x8B\x70\x2C\x8B\x1F\x3B\x9E\x00\x05\x00\x00", "xxxxxxxxxxxxxx", -0x5B, Priority::P1, PatternType::Func),
    PAT("DecreaseAttribute", DecreaseAttribute, "\x8B\x8A\xA8\x00\x00\x00\x89\x48\x0C\x5D\xC3\xCC", "xxxxxxxxxxxx", 0x18, Priority::P1, PatternType::Func),

    // ===== Trade (P1) =====
    PAT("Transaction",    Transaction,    "\x85\xFF\x74\x1D\x8B\x4D\x14\xEB\x08", "xxxxxxxxx", -0x7F, Priority::P1, PatternType::Func),
    PAT("BuyItemBase",    BuyItemBase,    "\xD9\xEE\xD9\x58\x0C\xC7\x40\x04",   "xxxxxxxx", 0xE,    Priority::P1, PatternType::Ptr),
    PAT("RequestQuote",   RequestQuote,   "\x8B\x75\x20\x83\xFE\x10\x76\x14",   "xxxxxxxx", -0x35,  Priority::P1, PatternType::Func),
    PAT("Salvage",        Salvage,
        "\x33\xC5\x89\x45\xFC\x8B\x45\x08\x89\x45\xF0\x8B\x45\x0C\x89\x45\xF4\x8B\x45\x10\x89\x45\xF8\x8D\x45\xEC\x50\x6A\x10\xC7\x45\xEC\x77",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", -0xB, Priority::P1, PatternType::Func),
    PAT("SalvageGlobal",  SalvageGlobal,  "\x8B\x4A\x04\x53\x89\x45\xF4\x8B\x42\x08", "xxxxxxxxxx", 0x0, Priority::P1, PatternType::Ptr),

    // ===== Agents (P0) =====
    PAT("ChangeTarget",   ChangeTarget,   "\x3B\xDF\x0F\x95",                   "xxxx",     -0x8A,  Priority::P0, PatternType::Func),
    PAT("CurrentTarget",  CurrentTarget,
        "\xFF\x35\x00\x00\x00\x00\xD9\x1D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x83\xC4\x08\x5F\x8B\xE5\x5D\xC3\xCC",
        "xx????xx????x????xxxxxxxxx", 0x2, Priority::P0, PatternType::Ptr),
    PAT("TargetLog",      TargetLog,      "\x53\x56\x57\x8B\xFA",                 "xxxxx",     0x0, Priority::P1, PatternType::Hook),

    // ===== Map (P0) =====
    PAT("Move",           Move,           "\x55\x8B\xEC\x83\xEC\x20\x8D\x45\xF0", "xxxxxxxxx", 0x0, Priority::P0, PatternType::Func),
    PAT("ClickCoords",    ClickCoords,    "\x8B\x45\x1C\x85\xC0\x74\x1C\xD9\x45\xF8", "xxxxxxxxxx", 0xC, Priority::P1, PatternType::Ptr),
    PAT("InstanceInfo",   InstanceInfo,   "\x6A\x2C\x50\xE8\x00\x00\x00\x00\x83\xC4\x08\xC7", "xxxx????xxxx", 0xD, Priority::P0, PatternType::Ptr),
    PAT("WorldConst",     WorldConst,     "\x8D\x04\x76\xC1\xE0\x04\x05",       "xxxxxxx",  0x7,    Priority::P1, PatternType::Ptr),
    PAT("Region",         Region,         "\x6A\x54\x8D\x46\x24\x89\x08",       "xxxxxxx",  -0x4,   Priority::P1, PatternType::Ptr),
    PAT("AreaInfo",       AreaInfo,       "\x6B\xC6\x7C\x5E\x05",               "xxxxx",    0x5,    Priority::P1, PatternType::Ptr),

    // ===== Trade UI (P1) =====
    PAT("TradeCancel",    TradeCancel,    "\xC7\x45\xFC\x01\x00\x00\x00\x50\x6A\x04", "xxxxxxxxxx", -0x7, Priority::P1, PatternType::Func),

    // ===== UI (P0/P1) =====
    PAT("UIMessage",      UIMessage,
        "\xB9\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x5D\xC3\x89\x45\x08",
        "x????x????xxxxx", -0x15, Priority::P0, PatternType::Func),
    PAT("Dialog",         Dialog,         "\x89\x4B\x24\x8B\x4B\x28\x83\xE9\x00", "xxxxxxxxx", 0x15, Priority::P0, PatternType::Func),
    PAT("CompassFlag",    CompassFlag,    "\x8D\x45\x10\x50\x56\x6A\x5D\x57",   "xxxxxxxx", 0x0,    Priority::P1, PatternType::Func),
    PAT("PartySearchButtonCallback", PartySearchButtonCallback,
        "\x8B\x45\x08\x83\xEC\x08\x56\x8B\xF1\x8B\x48\x04\x83\xF9\x0E",
        "xxxxxxxxxxxxxxx", -0x3, Priority::P1, PatternType::Func),
    PAT("PartyWindowButtonCallback", PartyWindowButtonCallback,
        "\x83\x7D\x08\x00\x57\x8B\xF9\x74\x11",
        "xxxxxxxxx", -0x3, Priority::P1, PatternType::Func),
    PAT("EnterMission",   EnterMission,   "\x83\xC9\x02\x89\x0A\x5D",           "xxxxxx",   0x23,   Priority::P1, PatternType::Func),
    PAT("SetDifficulty",  SetDifficulty,  "\x83\xC4\x1C\x68\x2A\x01\x00\x10",   "xxxxxxxx", 0x8B,   Priority::P1, PatternType::Func),
    PAT("OpenChest",      OpenChest,      "\x83\xC9\x01\x89\x4B\x24",           "xxxxxx",   0x28,   Priority::P1, PatternType::Func),
    PAT("AiMode",         AiMode,         "\x68\x3A\x00\x00\x10\xFF\x36",       "xxxxxxx",  0x0,    Priority::P1, PatternType::Ptr),
    PAT("HeroCommand",    HeroCommand,    "\x33\xD2\x68\xE0\x01\x00\x00",       "xxxxxxx",  0x0,    Priority::P1, PatternType::Ptr),
    PAT("HeroSkills",     HeroSkills,     "\x8B\x4E\x04\x50\x51\x85\xFF",       "xxxxxxx",  0x0,    Priority::P1, PatternType::Ptr),

    // ===== Party (P1) =====
    ASSERT_PAT("PlayerAdd",  PlayerAdd,  0, Priority::P1, PatternType::Ptr, "P:\\Code\\Gw\\Ui\\Game\\Party\\PtInvite.cpp", "m_invitePlayerId"),
    ASSERT_PAT("PlayerKick", PlayerKick, 0, Priority::P1, PatternType::Ptr, "P:\\Code\\Gw\\Ui\\Game\\Party\\PtUtil.cpp",   "playerId == MissionCliGetPlayerId()"),
    PAT("PartyInvitations", PartyInvitations, "\x8B\x7D\x0C\x8B\xF0\x83\xC4\x04\x8B\x47\x04", "xxxxxxxxxxx", 0x0, Priority::P1, PatternType::Ptr),

    // ===== Quest (P1) =====
    PAT("ActiveQuest",    ActiveQuest,    "\x8B\x45\x08\x3B\x46\x04\x0F\x84\x2D\x01\x00\x00", "xxxxxxxxxxxx", 0x0, Priority::P1, PatternType::Ptr),

    // ===== Hooks (P1) =====
    PAT("Engine",         Engine,         "\x56\x8B\x30\x85\xF6\x74\x78\xEB\x03\x8D\x49\x00\xD9\x46\x0C", "xxxxxxxxxxxxxxx", -0x23, Priority::P1, PatternType::Hook),
    PAT("Render",         Render,         "\xF6\xC4\x01\x74\x1C\x68",           "xxxxxx",   -0x69,  Priority::P1, PatternType::Hook),
    PAT("LoadFinished",   LoadFinished,   "\x2B\xD9\xC1\xE3\x03",               "xxxxx",    0x9F,   Priority::P1, PatternType::Hook),
    PAT("Trader",         Trader,         "\x8D\x4D\xFC\x51\x57\x6A\x56\x50",   "xxxxxxxx", -0x3D,  Priority::P1, PatternType::Hook),
    PAT("TradePartner",   TradePartner,   "\x6A\x00\x8D\x45\xF8\xC7\x45\xF8\x01\x00\x00\x00", "xxxxxxxxxxxx", -0xD, Priority::P1, PatternType::Hook),

    // ===== Text (P1) =====
    ASSERT_PAT("ValidateAsyncDecodeStr", ValidateAsyncDecodeStr, 0, Priority::P1, PatternType::Func, "P:\\Code\\Engine\\Text\\TextApi.cpp", "codedString"),

    // ===== Chat (P2) =====
    PAT("PostMessage",    PostMessage,    "\x6A\xFF\x6A\x00\x68\x01\x80",       "xxxxxxx",  0x18,   Priority::P2, PatternType::Ptr),
    PAT("ChatLog",        ChatLog,        "\x8B\x45\x08\x83\x7D\x0C\x07",       "xxxxxxx",  -0x21,  Priority::P2, PatternType::Hook),

    // ===== Camera (P2) =====
    PAT("CameraClass",    CameraClass,    "\xD9\xEE\xB9\x00\x00\x00\x00\xD9\x55\xFC", "xxx????xxx", 0x3, Priority::P2, PatternType::Ptr),
    PAT("FogPatch",       FogPatch,       "\x83\xE0\x01\x8B\x09\x50\x6A\x1C",         "xxxxxxxx",   0x2, Priority::P2, PatternType::Ptr),

    // ===== Frame UI (P0 — new for GWA3, NOT from AutoIt ASM scanner) =====
    PAT("SendFrameUIMsg", SendFrameUIMsg, "\x83\xC1\xDC\xE8",                   "xxxx",     0x3,    Priority::P0, PatternType::Func),
};

#undef PAT
#undef ASSERT_PAT

static constexpr int PATTERN_COUNT = sizeof(s_patterns) / sizeof(s_patterns[0]);

static void PostProcessOffsets(); // forward decl

static bool s_resolved = false;
static int s_resolvedCount = 0;
static int s_failedCount = 0;

bool ResolveAll() {
    if (!Scanner::IsInitialized()) {
        Log::Error("Offsets: Scanner not initialized");
        return false;
    }

    s_resolvedCount = 0;
    s_failedCount = 0;
    bool criticalFail = false;

    for (int i = 0; i < PATTERN_COUNT; i++) {
        const auto& p = s_patterns[i];
        uintptr_t result = 0;

        if (p.assertFile && p.assertMsg) {
            // Assertion-based scan
            result = Scanner::FindAssertion(p.assertFile, p.assertMsg, p.offset);
        } else if (p.bytes && p.mask) {
            result = Scanner::Find(p.bytes, p.mask, p.offset);
        }

        // Special post-processing for SendFrameUIMsg: resolve the E8 CALL
        if (result && strcmp(p.name, "SendFrameUIMsg") == 0) {
            result = Scanner::FunctionFromNearCall(result);
        }

        if (result) {
            *p.target = result;
            s_resolvedCount++;
            Log::Info("Offsets: [OK] %-30s = 0x%08X", p.name, result);
        } else {
            s_failedCount++;
            const char* priStr = (p.priority == Priority::P0) ? "P0" :
                                 (p.priority == Priority::P1) ? "P1" : "P2";
            Log::Warn("Offsets: [FAIL] %-28s (%s)", p.name, priStr);
            if (p.priority == Priority::P0 || p.priority == Priority::P1) {
                criticalFail = true;
            }
        }
    }

    Log::Info("Offsets: Resolved %d/%d patterns (%d failed)",
              s_resolvedCount, PATTERN_COUNT, s_failedCount);

    // Post-process: dereference ptr-type offsets to get runtime data pointers.
    PostProcessOffsets();

    s_resolved = !criticalFail;
    return s_resolved;
}

// Helper: read a uint32 at address (in-process, so just dereference)
static uintptr_t Deref(uintptr_t addr) {
    if (addr < 0x10000) return 0;
    return *reinterpret_cast<uintptr_t*>(addr);
}

static void PostProcessOffsets() {
    Log::Info("Offsets: Post-processing (dereferencing ptr-type patterns)...");

    // Core pointers: scan result contains address of code that references the data pointer
    if (BasePointer)    BasePointer    = Deref(BasePointer);
    if (Ping)           Ping           = Deref(Ping);
    if (StatusCode)     StatusCode     = Deref(StatusCode);
    if (PacketLocation) PacketLocation = Deref(PacketLocation);
    if (ActionBase)     ActionBase     = Deref(ActionBase);

    // PreGame: offset +0x35 from assertion site, then deref
    if (PreGame)        PreGame        = Deref(PreGame + 0x35);
    // FrameArray: assertion_site - 0x14 contains embedded data address
    // (AutoIt uses -0x13 but our FindAssertion returns BA addr, not BA-1)
    if (FrameArray) {
        uintptr_t val = Deref(FrameArray - 0x14);
        if (val > 0x10000 && val < 0x80000000) {
            Log::Info("Offsets: FrameArray deref at -0x14 -> 0x%08X", val);
            FrameArray = val;
        } else {
            Log::Warn("Offsets: FrameArray deref at -0x14 gave 0x%08X, keeping raw", val);
        }
    }
    // SceneContext: offset +0x1B from scan, then deref
    if (SceneContext) {
        uintptr_t ctx = Deref(SceneContext + 0x1B);
        SceneContext = ctx;
    }

    // Skills
    if (SkillBase)      SkillBase      = Deref(SkillBase);
    if (SkillTimer)     SkillTimer     = Deref(SkillTimer);

    // Agents
    if (AgentBase)      AgentBase      = Deref(AgentBase);
    if (MyID)           MyID           = Deref(MyID);
    if (CurrentTarget)  CurrentTarget  = Deref(CurrentTarget);

    // Map
    if (InstanceInfo)   InstanceInfo   = Deref(InstanceInfo);
    if (WorldConst)     WorldConst     = Deref(WorldConst);
    if (ClickCoords)    ClickCoords    = Deref(ClickCoords);
    if (Region)         Region         = Deref(Region);
    if (AreaInfo)       AreaInfo        = Deref(AreaInfo);

    // Attributes
    if (AttributeInfo)  AttributeInfo  = Deref(AttributeInfo);

    // Trade
    if (BuyItemBase)    BuyItemBase    = Deref(BuyItemBase);
    if (SalvageGlobal)  SalvageGlobal  = Deref(SalvageGlobal - 0x4);

    // UI: Dialog and OpenChest need E8 call resolution
    if (Dialog)         Dialog         = Scanner::FunctionFromNearCall(Dialog);
    if (OpenChest)      OpenChest      = Scanner::FunctionFromNearCall(OpenChest);
    if (SetDifficulty)  SetDifficulty  = Scanner::FunctionFromNearCall(SetDifficulty);
    if (EnterMission)   EnterMission   = Scanner::FunctionFromNearCall(EnterMission);

    // ChangeTarget: AutoIt adds +1 to the scan result
    if (ChangeTarget)   ChangeTarget   = ChangeTarget + 1;

    // Camera: scan+3 points at embedded pointer to Camera struct
    if (CameraClass)    CameraClass    = Deref(CameraClass);

    // TitleClientData: scan .rdata for the first entry pattern (00 00 00 00 23 40 00 00)
    // Result - 4 bytes = array base (from GWCA gwca.dll binary analysis)
    {
        auto rdata = Scanner::GetRdataSection();
        if (rdata.start && rdata.size) {
            uintptr_t found = Scanner::FindInRange(
                "\x00\x00\x00\x00\x23\x40\x00\x00",
                "xxxxxxxx", -4, rdata.start, rdata.size);
            if (found > 0x10000) {
                TitleClientDataBase = found;
                Log::Info("Offsets: TitleClientDataBase = 0x%08X (rdata scan)", found);
            } else {
                Log::Warn("Offsets: TitleClientData rdata scan failed");
            }
        }
    }

    // FriendList: complex post-processing (FindInRange + deref) — skip for now
    // RemoveFriend: complex post-processing (FindInRange + ResolveBranch) — skip for now

    Log::Info("Offsets: Post-processing complete");
    Log::Info("Offsets: BasePointer=0x%08X MyID=0x%08X AgentBase=0x%08X InstanceInfo=0x%08X",
              BasePointer, MyID, AgentBase, InstanceInfo);
}

bool IsResolved()       { return s_resolved; }
int GetResolvedCount()  { return s_resolvedCount; }
int GetFailedCount()    { return s_failedCount; }

} // namespace GWA3::Offsets
