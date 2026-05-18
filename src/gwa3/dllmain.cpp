#include <Windows.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/CrashDiag.h>
#include <gwa3/core/HookMarker.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/RuntimeWatchdog.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/core/TargetLogHook.h>
#include <gwa3/core/DialogHook.h>
#include <gwa3/core/Memory.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/MerchantMgr.h>
#include <gwa3/managers/FriendListMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/managers/CameraMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/ChatLogMgr.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/game/QuestIds.h>
#include <bots/common/BotFramework.h>
#include <bots/common/BotModuleRegistry.h>
#include <bots/common/BotModuleSelector.h>
#include <bots/froggy/FroggyHM.h>
#include <bots/arachnis_haunt/ArachnisHauntBot.h>
#include <bots/ravens_point/RavensPointBot.h>
#include <bots/rragars_menagerie/RragarsMenagerieBot.h>
#include <gwa3/utils/StringEncoding.h>
#include <gwa3/utils/EncStringCache.h>
#include <gwa3/llm/LlmBridge.h>
#include <gwa3/llm/IpcServer.h>
#include <gwa3/llm/GameSnapshot.h>

#include <cstdio>
#include <cstring>
#include <cwchar>

static HMODULE g_hModule = nullptr;

static bool CheckFlag(const char* envVar, const char* flagFile) {
    char envBuf[16] = {};
    if (GetEnvironmentVariableA(envVar, envBuf, sizeof(envBuf)) > 0) return true;
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&CheckFlag), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, flagFile);
    DWORD attr = GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES) {
        DeleteFileA(path);
        return true;
    }
    return false;
}

static bool ReadModeTextFile(const char* fileName, char* out, size_t outSize) {
    if (fileName == nullptr || out == nullptr || outSize == 0u) {
        return false;
    }

    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&ReadModeTextFile), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, fileName);

    FILE* f = nullptr;
    if (fopen_s(&f, path, "r") != 0 || f == nullptr) {
        return false;
    }
    const bool read = fgets(out, static_cast<int>(outSize), f) != nullptr;
    fclose(f);
    DeleteFileA(path);
    if (!read) {
        out[0] = '\0';
        return false;
    }

    out[outSize - 1u] = '\0';
    for (char* p = out; *p != '\0'; ++p) {
        if (*p == '\r' || *p == '\n') {
            *p = '\0';
            break;
        }
    }
    return out[0] != '\0';
}

static GWA3::Bot::BotModuleKind ResolveSelectedBotModule() {
    char moduleName[64] = {};
    if (GetEnvironmentVariableA("GWA3_BOT_MODULE", moduleName, sizeof(moduleName)) == 0) {
        (void)ReadModeTextFile("gwa3_bot_module.txt", moduleName, sizeof(moduleName));
    }
    return GWA3::Bot::ParseBotModuleName(moduleName);
}

static uint32_t ReadMyIdRaw() {
    if (GWA3::Offsets::MyID < 0x10000) return 0;
    return *reinterpret_cast<uint32_t*>(GWA3::Offsets::MyID);
}

static bool IsInGame() {
    return GWA3::MapMgr::GetMapId() > 0 && ReadMyIdRaw() > 0;
}

static bool WaitForLoadedMap(uint32_t mapId, DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (GWA3::MapMgr::GetMapId() == mapId &&
            GWA3::MapMgr::GetLoadingState() == 1u &&
            GWA3::MapMgr::GetIsMapLoaded() &&
            GWA3::AgentMgr::GetMyId() > 0u &&
            GWA3::AgentMgr::GetMyAgent() != nullptr) {
            return true;
        }
        Sleep(250);
    }
    return false;
}

static bool TravelToRataSumForArachnisCleanStart(const char* context) {
    constexpr uint32_t kAsiaJapanRegion = 4u;
    constexpr uint32_t kDistrictDefault = 0u;
    constexpr uint32_t kLanguageEnglish = 0u;
    constexpr uint32_t kAmericaRegion = 0u;

    if (GWA3::MapMgr::GetMapId() == GWA3::MapIds::RATA_SUM) {
        return WaitForLoadedMap(GWA3::MapIds::RATA_SUM, 15000u);
    }

    const uint32_t originMap = GWA3::MapMgr::GetMapId();
    if (!WaitForLoadedMap(originMap, 15000u)) {
        GWA3::Log::Warn("[INTG] Arachnis clean start current map not fully ready before travel "
                        "context=%s currentMap=%u loading=%u",
                        context ? context : "",
                        GWA3::MapMgr::GetMapId(),
                        GWA3::MapMgr::GetLoadingState());
    }
    Sleep(1500);

    GWA3::Log::Info("[INTG] Arachnis clean start traveling to Rata Sum via Asia/Japan English "
                    "from map=%u context=%s",
                    GWA3::MapMgr::GetMapId(),
                    context ? context : "");
    GWA3::MapMgr::Travel(GWA3::MapIds::RATA_SUM,
                         kAsiaJapanRegion,
                         kDistrictDefault,
                         kLanguageEnglish);
    const DWORD preferredStart = GetTickCount();
    while ((GetTickCount() - preferredStart) < 60000u) {
        if (GWA3::MapMgr::GetMapId() == GWA3::MapIds::RATA_SUM &&
            WaitForLoadedMap(GWA3::MapIds::RATA_SUM, 15000u)) {
            return true;
        }
        Sleep(500);
    }

    GWA3::Log::Warn("[INTG] Arachnis clean start Asia/Japan Rata Sum did not load; "
                    "falling back to default America English currentMap=%u loading=%u",
                    GWA3::MapMgr::GetMapId(),
                    GWA3::MapMgr::GetLoadingState());
    GWA3::MapMgr::Travel(GWA3::MapIds::RATA_SUM,
                         kAmericaRegion,
                         kDistrictDefault,
                         kLanguageEnglish);
    const DWORD fallbackStart = GetTickCount();
    while ((GetTickCount() - fallbackStart) < 60000u) {
        if (GWA3::MapMgr::GetMapId() == GWA3::MapIds::RATA_SUM &&
            WaitForLoadedMap(GWA3::MapIds::RATA_SUM, 15000u)) {
            return true;
        }
        Sleep(500);
    }
    return false;
}

static int RunArachnisHauntFlagMode() {
    GWA3::Log::Info("=== ARACHNIS HAUNT FEATURE TEST MODE ===");
    GWA3::RuntimeWatchdog::Start();

    if (IsInGame() && GWA3::MapMgr::GetMapId() != GWA3::MapIds::RATA_SUM) {
        GWA3::Log::Info("[INTG] Arachnis forcing clean start from Rata Sum currentMap=%u",
                        GWA3::MapMgr::GetMapId());
        (void)TravelToRataSumForArachnisCleanStart("feature-test-clean-start");
    }

    if (IsInGame() &&
        (GWA3::MapMgr::GetMapId() != GWA3::MapIds::RATA_SUM ||
         !WaitForLoadedMap(GWA3::MapIds::RATA_SUM, 15000u))) {
        GWA3::Log::Error("[INTG] Arachnis clean start failed currentMap=%u",
                         GWA3::MapMgr::GetMapId());
        GWA3::RuntimeWatchdog::Stop();
        GWA3::Log::Info("=== ARACHNIS HAUNT FEATURE TEST COMPLETE ===");
        GWA3::Log::Info("[INTG] Arachnis Haunt feature test complete: 1 failures");
        return 1;
    }

    GWA3::Bot::ArachnisHauntBot::Register();
    GWA3::Bot::Start();
    const bool botStarted = GWA3::Bot::IsRunning();
    GWA3::Log::Info("[INTG] Arachnis bot thread started: %d", botStarted ? 1 : 0);
    if (botStarted) {
        GWA3::Bot::SetState(IsInGame() ? GWA3::Bot::BotState::InTown
                                       : GWA3::Bot::BotState::CharSelect);
    }

    bool sawMagusStones = GWA3::MapMgr::GetMapId() == GWA3::MapIds::MAGUS_STONES;
    bool sawLevel1 = GWA3::MapMgr::GetMapId() == GWA3::MapIds::ARACHNIS_HAUNT_LVL1;
    bool sawLevel2 = GWA3::MapMgr::GetMapId() == GWA3::MapIds::ARACHNIS_HAUNT_LVL2;
    bool returnedToMagusAfterReward = false;
    bool sawErrorState = false;
    int level1EntryCount = 0;

    uint32_t lastMapId = 0xFFFFFFFFu;
    GWA3::Bot::BotState lastState = GWA3::Bot::BotState::Idle;
    DWORD lastProgressLog = 0u;
    const DWORD start = GetTickCount();
    constexpr DWORD kTimeoutMs = 7200000u;

    while (botStarted && (GetTickCount() - start) < kTimeoutMs) {
        const uint32_t mapId = GWA3::MapMgr::GetMapId();
        const GWA3::Bot::BotState state = GWA3::Bot::GetState();

        sawMagusStones = sawMagusStones || mapId == GWA3::MapIds::MAGUS_STONES;
        sawLevel1 = sawLevel1 || mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL1;
        sawLevel2 = sawLevel2 || mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL2;
        if (lastMapId != mapId && mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL1) {
            ++level1EntryCount;
        }
        if (sawLevel2 && mapId == GWA3::MapIds::MAGUS_STONES) {
            returnedToMagusAfterReward = true;
        }
        sawErrorState = sawErrorState || state == GWA3::Bot::BotState::Error;

        const DWORD nowTicks = GetTickCount();
        if (mapId != lastMapId || state != lastState || (nowTicks - lastProgressLog) >= 10000u) {
            GWA3::Log::Info(
                "[INTG] Arachnis progress: map=%u state=%d heroes=%u lvl1Entries=%d level2=%d returned=%d elapsed=%us",
                mapId,
                static_cast<int>(state),
                GWA3::PartyMgr::CountPartyHeroes(),
                level1EntryCount,
                sawLevel2 ? 1 : 0,
                returnedToMagusAfterReward ? 1 : 0,
                static_cast<unsigned>((nowTicks - start) / 1000u));
            lastMapId = mapId;
            lastState = state;
            lastProgressLog = nowTicks;
        }

        if (GWA3::RuntimeWatchdog::FailureDetected()) {
            GWA3::Log::Error("[INTG] Arachnis feature test aborted: %s",
                             GWA3::RuntimeWatchdog::FailureReason());
            break;
        }
        if (sawErrorState || returnedToMagusAfterReward) {
            break;
        }
        Sleep(1000);
    }

    int failures = 0;
    const auto check = [&failures](const char* label, bool passed) {
        GWA3::Log::Info("[INTG] %s %s", passed ? "[PASS]" : "[FAIL]", label);
        if (!passed) {
            ++failures;
        }
    };
    check("Reached Magus Stones", sawMagusStones);
    check("Entered Arachnis level 1", sawLevel1);
    check("Entered Arachnis level 1 twice for reward bounce", level1EntryCount >= 2);
    check("Entered Arachnis level 2", sawLevel2);
    check("Returned to Magus Stones after reward hand-in", returnedToMagusAfterReward);
    check("Bot avoided Error state", !sawErrorState);
    check("Runtime watchdog stayed clear", !GWA3::RuntimeWatchdog::FailureDetected());

    if (!returnedToMagusAfterReward && !sawErrorState &&
        !GWA3::RuntimeWatchdog::FailureDetected() &&
        botStarted && (GetTickCount() - start) >= kTimeoutMs) {
        GWA3::Log::Error("[INTG] Arachnis feature test timed out before returning to Magus Stones");
        ++failures;
    }

    if (GWA3::Bot::IsRunning()) {
        GWA3::Bot::Stop();
    }

    GWA3::Log::Info("=== ARACHNIS HAUNT FEATURE TEST COMPLETE ===");
    GWA3::Log::Info("[INTG] Arachnis Haunt feature test complete: %d failures", failures);
    return failures;
}

static int RunRavensPointFlagMode() {
    GWA3::Log::Info("=== RAVEN'S POINT FEATURE TEST MODE ===");
    GWA3::RuntimeWatchdog::Start();

    GWA3::Bot::RavensPointBot::Register();
    GWA3::Bot::Start();
    const bool botStarted = GWA3::Bot::IsRunning();
    GWA3::Log::Info("[INTG] Raven bot thread started: %d", botStarted ? 1 : 0);
    if (botStarted) {
        GWA3::Bot::SetState(IsInGame() ? GWA3::Bot::BotState::InTown
                                       : GWA3::Bot::BotState::CharSelect);
    }

    bool sawOlafstead = GWA3::MapMgr::GetMapId() == GWA3::MapIds::OLAFSTEAD;
    bool sawVarajar = GWA3::MapMgr::GetMapId() == GWA3::MapIds::VARAJAR_FELLS_1;
    bool sawLevel1 = GWA3::MapMgr::GetMapId() == GWA3::MapIds::RAVENS_POINT_LVL1;
    bool sawLevel2 = GWA3::MapMgr::GetMapId() == GWA3::MapIds::RAVENS_POINT_LVL2;
    bool sawLevel3 = GWA3::MapMgr::GetMapId() == GWA3::MapIds::RAVENS_POINT_LVL3;
    bool returnedToVarajarAfterLevel3 = false;
    bool sawErrorState = false;

    uint32_t lastMapId = 0xFFFFFFFFu;
    GWA3::Bot::BotState lastState = GWA3::Bot::BotState::Idle;
    DWORD lastProgressLog = 0u;
    const DWORD start = GetTickCount();
    constexpr DWORD kTimeoutMs = 3600000u;

    while (botStarted && (GetTickCount() - start) < kTimeoutMs) {
        const uint32_t mapId = GWA3::MapMgr::GetMapId();
        const GWA3::Bot::BotState state = GWA3::Bot::GetState();

        sawOlafstead = sawOlafstead || mapId == GWA3::MapIds::OLAFSTEAD;
        sawVarajar = sawVarajar || mapId == GWA3::MapIds::VARAJAR_FELLS_1;
        sawLevel1 = sawLevel1 || mapId == GWA3::MapIds::RAVENS_POINT_LVL1;
        sawLevel2 = sawLevel2 || mapId == GWA3::MapIds::RAVENS_POINT_LVL2;
        sawLevel3 = sawLevel3 || mapId == GWA3::MapIds::RAVENS_POINT_LVL3;
        returnedToVarajarAfterLevel3 =
            returnedToVarajarAfterLevel3 ||
            (sawLevel3 && mapId == GWA3::MapIds::VARAJAR_FELLS_1);
        sawErrorState = sawErrorState || state == GWA3::Bot::BotState::Error;

        const DWORD nowTicks = GetTickCount();
        if (mapId != lastMapId || state != lastState || (nowTicks - lastProgressLog) >= 10000u) {
            const auto* ravenQuest = GWA3::QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT);
            GWA3::Log::Info(
                "[INTG] Raven progress: map=%u state=%d heroes=%u activeQuest=0x%X ravenPresent=%d "
                "ravenLogState=%u level2=%d level3=%d returned=%d elapsed=%us",
                mapId,
                static_cast<int>(state),
                GWA3::PartyMgr::CountPartyHeroes(),
                GWA3::QuestMgr::GetActiveQuestId(),
                ravenQuest != nullptr ? 1 : 0,
                ravenQuest ? ravenQuest->log_state : 0u,
                sawLevel2 ? 1 : 0,
                sawLevel3 ? 1 : 0,
                returnedToVarajarAfterLevel3 ? 1 : 0,
                static_cast<unsigned>((nowTicks - start) / 1000u));
            lastMapId = mapId;
            lastState = state;
            lastProgressLog = nowTicks;
        }

        if (GWA3::RuntimeWatchdog::FailureDetected()) {
            GWA3::Log::Error("[INTG] Raven feature test aborted: %s",
                             GWA3::RuntimeWatchdog::FailureReason());
            break;
        }
        if (sawErrorState || returnedToVarajarAfterLevel3) {
            break;
        }
        Sleep(1000);
    }

    int failures = 0;
    const auto check = [&failures](const char* label, bool passed) {
        GWA3::Log::Info("[INTG] %s %s", passed ? "[PASS]" : "[FAIL]", label);
        if (!passed) {
            ++failures;
        }
    };
    check("Reached Olafstead", sawOlafstead);
    check("Reached Varajar Fells", sawVarajar);
    check("Entered Ravens Point level 1", sawLevel1);
    check("Entered Ravens Point level 2", sawLevel2);
    check("Entered Ravens Point level 3", sawLevel3);
    check("Returned to Varajar after Raven clear", returnedToVarajarAfterLevel3);
    check("Bot avoided Error state", !sawErrorState);
    check("Runtime watchdog stayed clear", !GWA3::RuntimeWatchdog::FailureDetected());

    if (!returnedToVarajarAfterLevel3 && !sawErrorState &&
        !GWA3::RuntimeWatchdog::FailureDetected() &&
        botStarted && (GetTickCount() - start) >= kTimeoutMs) {
        GWA3::Log::Error("[INTG] Raven feature test timed out before completing and returning to Varajar");
        ++failures;
    }

    if (GWA3::Bot::IsRunning()) {
        GWA3::Bot::Stop();
    }

    GWA3::Log::Info("=== RAVENS POINT FEATURE TEST COMPLETE ===");
    GWA3::Log::Info("[INTG] Ravens Point feature test complete: %d failures", failures);
    return failures;
}

static int RunRragarsMenagerieFlagMode() {
    GWA3::Log::Info("=== RRAGARS MENAGERIE FEATURE TEST MODE ===");
    GWA3::RuntimeWatchdog::Start();

    GWA3::Bot::RragarsMenagerieBot::Register();
    GWA3::Bot::Start();
    const bool botStarted = GWA3::Bot::IsRunning();
    GWA3::Log::Info("[INTG] Rragars bot thread started: %d", botStarted ? 1 : 0);
    if (botStarted) {
        GWA3::Bot::SetState(IsInGame() ? GWA3::Bot::BotState::InTown
                                       : GWA3::Bot::BotState::CharSelect);
    }

    bool sawDoomlore = GWA3::MapMgr::GetMapId() == GWA3::MapIds::DOOMLORE_SHRINE;
    bool sawDalada = GWA3::MapMgr::GetMapId() == GWA3::MapIds::DALADA_UPLANDS;
    bool sawGrothmar = GWA3::MapMgr::GetMapId() == GWA3::MapIds::GROTHMAR_WARDOWNS;
    bool sawSacnoth = GWA3::MapMgr::GetMapId() == GWA3::MapIds::SACNOTH_VALLEY;
    bool sawLevel1 = GWA3::MapMgr::GetMapId() == GWA3::MapIds::RRAGARS_MENAGERIE_LVL1;
    bool sawLevel2 = GWA3::MapMgr::GetMapId() == GWA3::MapIds::RRAGARS_MENAGERIE_LVL2;
    bool sawLevel3 = GWA3::MapMgr::GetMapId() == GWA3::MapIds::RRAGARS_MENAGERIE_LVL3;
    bool returnedToDoomloreAfterReward = false;
    bool sawErrorState = false;

    uint32_t lastMapId = 0xFFFFFFFFu;
    GWA3::Bot::BotState lastState = GWA3::Bot::BotState::Idle;
    DWORD lastProgressLog = 0u;
    const DWORD start = GetTickCount();
    constexpr DWORD kTimeoutMs = 5400000u;

    while (botStarted && (GetTickCount() - start) < kTimeoutMs) {
        const uint32_t mapId = GWA3::MapMgr::GetMapId();
        const GWA3::Bot::BotState state = GWA3::Bot::GetState();

        sawDoomlore = sawDoomlore || mapId == GWA3::MapIds::DOOMLORE_SHRINE;
        sawDalada = sawDalada || mapId == GWA3::MapIds::DALADA_UPLANDS;
        sawGrothmar = sawGrothmar || mapId == GWA3::MapIds::GROTHMAR_WARDOWNS;
        sawSacnoth = sawSacnoth || mapId == GWA3::MapIds::SACNOTH_VALLEY;
        sawLevel1 = sawLevel1 || mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL1;
        sawLevel2 = sawLevel2 || mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL2;
        sawLevel3 = sawLevel3 || mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL3;
        returnedToDoomloreAfterReward =
            returnedToDoomloreAfterReward ||
            (sawLevel3 && mapId == GWA3::MapIds::DOOMLORE_SHRINE);
        sawErrorState = sawErrorState || state == GWA3::Bot::BotState::Error;

        const DWORD nowTicks = GetTickCount();
        if (mapId != lastMapId || state != lastState || (nowTicks - lastProgressLog) >= 10000u) {
            GWA3::Log::Info(
                "[INTG] Rragars progress: map=%u state=%d heroes=%u level1=%d level2=%d level3=%d returned=%d elapsed=%us",
                mapId,
                static_cast<int>(state),
                GWA3::PartyMgr::CountPartyHeroes(),
                sawLevel1 ? 1 : 0,
                sawLevel2 ? 1 : 0,
                sawLevel3 ? 1 : 0,
                returnedToDoomloreAfterReward ? 1 : 0,
                static_cast<unsigned>((nowTicks - start) / 1000u));
            lastMapId = mapId;
            lastState = state;
            lastProgressLog = nowTicks;
        }

        if (GWA3::RuntimeWatchdog::FailureDetected()) {
            GWA3::Log::Error("[INTG] Rragars feature test aborted: %s",
                             GWA3::RuntimeWatchdog::FailureReason());
            break;
        }
        if (sawErrorState || returnedToDoomloreAfterReward) {
            break;
        }
        Sleep(1000);
    }

    int failures = 0;
    auto check = [&](const char* label, bool ok) {
        GWA3::Log::Info("[INTG] %s: %s", label, ok ? "PASS" : "FAIL");
        if (!ok) ++failures;
    };
    check("Reached Doomlore Shrine", sawDoomlore);
    check("Reached Dalada Uplands", sawDalada);
    check("Reached Grothmar Wardowns", sawGrothmar);
    check("Reached Sacnoth Valley", sawSacnoth);
    check("Entered Rragars level 1", sawLevel1);
    check("Entered Rragars level 2", sawLevel2);
    check("Entered Rragars level 3", sawLevel3);
    check("Returned to Doomlore after reward chest", returnedToDoomloreAfterReward);
    check("Bot avoided Error state", !sawErrorState);
    check("Runtime watchdog stayed clear", !GWA3::RuntimeWatchdog::FailureDetected());

    if (!returnedToDoomloreAfterReward && !sawErrorState &&
        !GWA3::RuntimeWatchdog::FailureDetected() &&
        botStarted && (GetTickCount() - start) >= kTimeoutMs) {
        GWA3::Log::Error("[INTG] Rragars feature test timed out before returning to Doomlore");
        ++failures;
    }

    if (GWA3::Bot::IsRunning()) {
        GWA3::Bot::Stop();
    }

    GWA3::Log::Info("=== RRAGARS MENAGERIE FEATURE TEST COMPLETE ===");
    GWA3::Log::Info("[INTG] Rragars Menagerie feature test complete: %d failures", failures);
    return failures;
}

static bool ClickPlayButtonMouseFallback() {
    HWND hwnd = static_cast<HWND>(GWA3::MemoryMgr::GetGWWindowHandle());
    if (!hwnd || !IsWindow(hwnd)) {
        GWA3::Log::Warn("Bootstrap: mouse Play fallback has no valid GW hwnd");
        return false;
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        GWA3::Log::Warn("Bootstrap: mouse Play fallback failed to read window rect");
        return false;
    }

    const LONG width = rect.right - rect.left;
    const LONG height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        GWA3::Log::Warn("Bootstrap: mouse Play fallback invalid window size %ldx%ld", width, height);
        return false;
    }

    const LONG playX = static_cast<LONG>(width * 0.73f);
    const LONG playY = static_cast<LONG>(height * 0.96f);
    const LONG loginX = static_cast<LONG>(width * 0.18f);
    const LONG loginY = static_cast<LONG>(height * 0.82f);
    const LONG loginAltX = static_cast<LONG>(width * 0.17f);
    const LONG loginAltY = static_cast<LONG>(height * 0.81f);

    const auto postClick = [&](LONG x, LONG y) -> bool {
        const LPARAM lparam = MAKELPARAM(static_cast<SHORT>(x), static_cast<SHORT>(y));
        const BOOL downOk = PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lparam);
        const BOOL upOk = PostMessageA(hwnd, WM_LBUTTONUP, 0, lparam);
        return downOk && upOk;
    };

    const BOOL enterDown = PostMessageA(hwnd, WM_KEYDOWN, VK_RETURN, 0);
    const BOOL enterChar = PostMessageA(hwnd, WM_CHAR, '\r', 0);
    const BOOL enterUp = PostMessageA(hwnd, WM_KEYUP, VK_RETURN, 0);
    const bool enterSent = enterDown && enterChar && enterUp;
    Sleep(100);

    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    BringWindowToTop(hwnd);
    Sleep(100);

    POINT oldPos{};
    GetCursorPos(&oldPos);

    const auto inputClick = [](LONG screenX, LONG screenY) -> bool {
        SetCursorPos(screenX, screenY);
        Sleep(50);
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        return SendInput(2, inputs, sizeof(INPUT)) == 2;
    };

    const auto inputEnter = []() -> bool {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_RETURN;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_RETURN;
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
        return SendInput(2, inputs, sizeof(INPUT)) == 2;
    };

    const bool enterInputSent = inputEnter();
    Sleep(100);
    const bool loginSent = postClick(loginX, loginY);
    Sleep(100);
    const bool loginAltSent = postClick(loginAltX, loginAltY);
    Sleep(100);
    const bool playSent = postClick(playX, playY);
    Sleep(100);
    const bool loginInputSent = inputClick(rect.left + loginX, rect.top + loginY);
    Sleep(100);
    const bool loginAltInputSent = inputClick(rect.left + loginAltX, rect.top + loginAltY);
    Sleep(100);
    const bool playInputSent = inputClick(rect.left + playX, rect.top + playY);
    SetCursorPos(oldPos.x, oldPos.y);

    GWA3::Log::Info("Bootstrap: Play fallback hwnd=0x%08X rect=(%ld,%ld,%ld,%ld) "
                    "enterSent=%u enterInput=%u loginClient=(%ld,%ld) loginSent=%u "
                    "loginAltClient=(%ld,%ld) loginAltSent=%u playClient=(%ld,%ld) playSent=%u "
                    "loginInput=%u loginAltInput=%u playInput=%u",
                    static_cast<unsigned>(reinterpret_cast<uintptr_t>(hwnd)),
                    rect.left, rect.top, rect.right, rect.bottom,
                    enterSent ? 1u : 0u,
                    enterInputSent ? 1u : 0u,
                    loginX, loginY, loginSent ? 1u : 0u,
                    loginAltX, loginAltY, loginAltSent ? 1u : 0u,
                    playX, playY,
                    playSent ? 1u : 0u,
                    loginInputSent ? 1u : 0u,
                    loginAltInputSent ? 1u : 0u,
                    playInputSent ? 1u : 0u);
    return enterSent || enterInputSent || loginSent || loginAltSent || playSent ||
           loginInputSent || loginAltInputSent || playInputSent;
}

static bool ClickReconnectNoIfVisible() {
    if (!GWA3::UIMgr::IsFrameVisible(GWA3::UIMgr::Hashes::ReconnectYes)) {
        return false;
    }

    GWA3::Log::Info("Bootstrap: reconnect dialog visible; selecting No for clean character login");
    if (GWA3::UIMgr::ButtonClickByHash(GWA3::UIMgr::Hashes::ReconnectNo)) {
        Sleep(1000);
        return true;
    }

    HWND hwnd = static_cast<HWND>(GWA3::MemoryMgr::GetGWWindowHandle());
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }

    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    BringWindowToTop(hwnd);
    Sleep(100);
    PostMessageA(hwnd, WM_KEYDOWN, VK_RIGHT, 0);
    PostMessageA(hwnd, WM_KEYUP, VK_RIGHT, 0);
    Sleep(100);
    PostMessageA(hwnd, WM_KEYDOWN, VK_RETURN, 0);
    PostMessageA(hwnd, WM_CHAR, '\r', 0);
    PostMessageA(hwnd, WM_KEYUP, VK_RETURN, 0);
    Sleep(1000);
    return true;
}

static bool IsLikelyUserAddress(uintptr_t address) {
    return address >= 0x10000u && address < 0x80000000u;
}

static bool ReadSafePtr(uintptr_t address, uintptr_t& value) {
    value = 0u;
    if (!IsLikelyUserAddress(address)) {
        return false;
    }
    __try {
        value = *reinterpret_cast<uintptr_t*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool ReadSafeU32(uintptr_t address, uint32_t& value) {
    value = 0u;
    if (!IsLikelyUserAddress(address)) {
        return false;
    }
    __try {
        value = *reinterpret_cast<uint32_t*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool IsLikelyPregameContext(uintptr_t preGamePtr, uintptr_t& charsPtr, uint32_t& charsCount) {
    charsPtr = 0u;
    charsCount = 0u;
    if (!IsLikelyUserAddress(preGamePtr)) {
        return false;
    }
    if (!ReadSafePtr(preGamePtr + 0x148u, charsPtr) ||
        !ReadSafeU32(preGamePtr + 0x14Cu, charsCount)) {
        return false;
    }
    return IsLikelyUserAddress(charsPtr) && charsCount > 0u && charsCount <= 28u;
}

static uintptr_t ResolvePregameContext(uintptr_t& charsPtr, uint32_t& charsCount) {
    charsPtr = 0u;
    charsCount = 0u;
    if (GWA3::Offsets::PreGame < 0x10000u) {
        return 0u;
    }

    uintptr_t candidate = 0u;
    if (ReadSafePtr(GWA3::Offsets::PreGame, candidate) &&
        IsLikelyPregameContext(candidate, charsPtr, charsCount)) {
        return candidate;
    }

    if (IsLikelyPregameContext(GWA3::Offsets::PreGame, charsPtr, charsCount)) {
        return GWA3::Offsets::PreGame;
    }
    return 0u;
}

static bool ReadPregameCharacterName(uintptr_t charsPtr, uint32_t index, wchar_t* out, size_t outCount) {
    if (out == nullptr || outCount == 0u || !IsLikelyUserAddress(charsPtr)) {
        return false;
    }
    out[0] = L'\0';
    constexpr uintptr_t kLoginCharacterSize = 44u;
    const uintptr_t nameAddr = charsPtr + (static_cast<uintptr_t>(index) * kLoginCharacterSize) + 4u;
    __try {
        wcsncpy_s(out, outCount, reinterpret_cast<const wchar_t*>(nameAddr), _TRUNCATE);
        out[outCount - 1u] = L'\0';
        return out[0] != L'\0';
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out[0] = L'\0';
        return false;
    }
}

static bool WritePregameCharacterIndex(uintptr_t preGamePtr, uint32_t index) {
    if (!IsLikelyUserAddress(preGamePtr)) {
        return false;
    }
    __try {
        *reinterpret_cast<uint32_t*>(preGamePtr + 0x124u) = index;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool SelectPregameCharacterByName(const char* characterName) {
    if (characterName == nullptr || characterName[0] == '\0' || IsInGame()) {
        return true;
    }

    wchar_t target[64] = {};
    int converted = MultiByteToWideChar(CP_UTF8, 0, characterName, -1, target, static_cast<int>(_countof(target)));
    if (converted <= 0) {
        converted = MultiByteToWideChar(CP_ACP, 0, characterName, -1, target, static_cast<int>(_countof(target)));
    }
    if (converted <= 0 || target[0] == L'\0') {
        GWA3::Log::Warn("Bootstrap: failed to convert target character name");
        return false;
    }

    uintptr_t charsPtr = 0u;
    uint32_t charsCount = 0u;
    const uintptr_t preGamePtr = ResolvePregameContext(charsPtr, charsCount);
    if (preGamePtr == 0u) {
        GWA3::Log::Warn("Bootstrap: PreGame character array unavailable for target %s offset=0x%08X",
                        characterName,
                        static_cast<unsigned>(GWA3::Offsets::PreGame));
        return false;
    }

    for (uint32_t i = 0u; i < charsCount; ++i) {
        wchar_t liveName[64] = {};
        if (!ReadPregameCharacterName(charsPtr, i, liveName, _countof(liveName))) {
            continue;
        }
        GWA3::Log::Info("Bootstrap: PreGame character[%u]=%S", i, liveName);
        if (_wcsicmp(liveName, target) == 0) {
            const bool wrote = WritePregameCharacterIndex(preGamePtr, i);
            GWA3::Log::Info("Bootstrap: selected target character %s index=%u wrote=%u",
                            characterName,
                            i,
                            wrote ? 1u : 0u);
            Sleep(500);
            return wrote;
        }
    }

    GWA3::Log::Warn("Bootstrap: target character %s not found in %u PreGame characters",
                    characterName,
                    charsCount);
    return false;
}

static bool RunCharSelectBootstrap(DWORD timeoutMs, const char* targetCharacterName = nullptr) {
    if (IsInGame()) {
        GWA3::Log::Info("Bootstrap: Already in game (MapID=%u MyID=%u)",
                        GWA3::MapMgr::GetMapId(), ReadMyIdRaw());
        return true;
    }

    const DWORD start = GetTickCount();
    DWORD lastLog = 0;
    DWORD lastPlayAttempt = 0;

    GWA3::Log::Info("Bootstrap: Entering pre-game phase");

    while (GetTickCount() - start < timeoutMs) {
        const uint32_t mapId = GWA3::MapMgr::GetMapId();
        const uint32_t myId = ReadMyIdRaw();
        if (mapId > 0 && myId > 0) {
            GWA3::Log::Info("Bootstrap: Map loaded (MapID=%u MyID=%u)", mapId, myId);
            return true;
        }

        const DWORD now = GetTickCount();
        if (now - lastLog >= 3000) {
            GWA3::Log::Info("Bootstrap: waiting for map load (MapID=%u MyID=%u hb=%u)",
                            mapId, myId,
                            GWA3::RenderHook::GetHeartbeat());
            lastLog = now;
        }

        if (ClickReconnectNoIfVisible()) {
            lastPlayAttempt = now;
            continue;
        }

        if (now - lastPlayAttempt >= 2500) {
            bool selectionOk = true;
            if (targetCharacterName != nullptr && targetCharacterName[0] != '\0') {
                selectionOk = SelectPregameCharacterByName(targetCharacterName);
            }
            if (selectionOk) {
                ClickPlayButtonMouseFallback();
            } else {
                GWA3::Log::Warn("Bootstrap: target character not confirmed; withholding Play click");
            }
            lastPlayAttempt = now;
        }

        Sleep(250);
    }

    GWA3::Log::Error("Bootstrap: timed out waiting for map load");
    return false;
}

static bool WaitForPlayerHydration(DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    DWORD lastLog = 0;

    while (GetTickCount() - start < timeoutMs) {
        const uint32_t mapId = GWA3::MapMgr::GetMapId();
        const uint32_t myId = ReadMyIdRaw();
        auto* me = GWA3::AgentMgr::GetMyAgent();

        float x = 0.0f;
        float y = 0.0f;
        bool havePos = false;
        uint32_t typeMap = 0;
        uint32_t modelState = 0;

        if (me) {
            x = me->x;
            y = me->y;
            havePos = !(x == 0.0f && y == 0.0f);
            typeMap = me->type_map;
            modelState = me->model_state;
        }

        const bool hydrated =
            mapId > 0 &&
            myId > 0 &&
            me != nullptr &&
            me->hp > 0.0f &&
            havePos &&
            (typeMap & 0x400000) != 0;
        if (hydrated) {
            GWA3::Log::Info("Bootstrap: Player hydrated (MapID=%u MyID=%u TypeMap=0x%X ModelState=%u Pos=(%.1f, %.1f))",
                            mapId, myId, typeMap, modelState, x, y);
            return true;
        }

        const DWORD now = GetTickCount();
        if (now - lastLog >= 2000) {
            GWA3::Log::Info("Bootstrap: waiting for player hydration (MapID=%u MyID=%u TypeMap=0x%X ModelState=%u Pos=(%.1f, %.1f))",
                            mapId, myId, typeMap, modelState, x, y);
            lastLog = now;
        }

        Sleep(250);
    }

    GWA3::Log::Warn("Bootstrap: player hydration timed out; continuing with partial readiness");
    return false;
}

DWORD WINAPI InitThread(LPVOID hModule) {
    GWA3::Log::Initialize();
    GWA3::Log::Info("gwa3.dll loaded at 0x%08X", static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(hModule)));

    // Install the top-level unhandled-exception filter and VEH *first* so
    // that any crash during subsequent hook installation or later runtime
    // writes a real exception-stream minidump (EIP, stack, registers, and
    // the faulting module offset) to the log. Without this, the watchdog's
    // post-dialog CaptureProcessState captures only the process state
    // while GW's crash dialog is already up â€” no exception data.
    GWA3::CrashDiag::Initialize();
    GWA3::HookMarker::SelfTest();

    bool llmMode = CheckFlag("GWA3_LLM_MODE", "gwa3_llm_mode.flag");
    bool llmAdvisory = CheckFlag("GWA3_LLM_ADVISORY", "gwa3_llm_advisory.flag");
    bool arachnisTest = CheckFlag("GWA3_TEST_ARACHNIS", "gwa3_test_arachnis.flag");
    bool ravensTest = CheckFlag("GWA3_TEST_RAVENS", "gwa3_test_ravens.flag");
    bool rragarsTest = CheckFlag("GWA3_TEST_RRAGARS", "gwa3_test_rragars.flag");
    char bootstrapCharacterName[64] = {};
    if (!ReadModeTextFile("gwa3_character_name.txt", bootstrapCharacterName, sizeof(bootstrapCharacterName)) &&
        arachnisTest) {
        strcpy_s(bootstrapCharacterName, "GWA3 SAMPLE FOUR");
    }
    GWA3::Log::Info("Mode flags: llm=%d advisory=%d arachnisTest=%d ravensTest=%d rragarsTest=%d",
                    llmMode, llmAdvisory, arachnisTest, ravensTest, rragarsTest);
    if (bootstrapCharacterName[0] != '\0') {
        GWA3::Log::Info("Bootstrap target character: %s", bootstrapCharacterName);
    }

    HMODULE gwModule = GetModuleHandleA(nullptr);
    if (!GWA3::Scanner::Initialize(gwModule)) {
        GWA3::Log::Error("Scanner initialization failed - aborting");
        return 1;
    }

    if (!GWA3::Offsets::ResolveAll()) {
        GWA3::Log::Warn("Some offsets failed to resolve - continuing with partial coverage");
    }

    // Stage memory patches using resolved offsets
    if (GWA3::Offsets::CameraUpdateBypass > 0x10000) {
        const uint8_t patch[2] = {0xEB, 0x0C}; // JMP +12 (skip float copy-back)
        GWA3::Memory::GetCameraUnlockPatch().SetPatch(GWA3::Offsets::CameraUpdateBypass, patch, 2);
        GWA3::Log::Info("CameraUpdateBypass patch staged at 0x%08X", GWA3::Offsets::CameraUpdateBypass);
    }
    if (GWA3::Offsets::LevelDataBypass > 0x10000) {
        const uint8_t patch = 0xEB; // JMP (unconditional)
        GWA3::Memory::GetLevelDataBypassPatch().SetPatch(GWA3::Offsets::LevelDataBypass, &patch, 1);
        GWA3::Log::Info("LevelDataBypass patch staged at 0x%08X", GWA3::Offsets::LevelDataBypass);
    }
    if (GWA3::Offsets::MapPortBypass > 0x10000) {
        const uint8_t patch[2] = {0x90, 0x90}; // NOP NOP
        GWA3::Memory::GetMapPortBypassPatch().SetPatch(GWA3::Offsets::MapPortBypass, patch, 2);
        GWA3::Log::Info("MapPortBypass patch staged at 0x%08X", GWA3::Offsets::MapPortBypass);
    }

    GWA3::CtoS::Initialize();
    GWA3::AgentMgr::Initialize();
    GWA3::SkillMgr::Initialize();
    GWA3::ItemMgr::Initialize();
    GWA3::MapMgr::Initialize();
    GWA3::PartyMgr::Initialize();
    GWA3::QuestMgr::Initialize();
    GWA3::ChatMgr::Initialize();
    GWA3::TradeMgr::Initialize();
    GWA3::MerchantMgr::Initialize();
    GWA3::FriendListMgr::Initialize();
    GWA3::UIMgr::Initialize();
    GWA3::MemoryMgr::Initialize();
    GWA3::PlayerMgr::Initialize();
    GWA3::CameraMgr::Initialize();
    GWA3::EffectMgr::Initialize();

    bool gameThreadOk = false;

    const bool alreadyInGameBeforeBootstrap = IsInGame();
    if (alreadyInGameBeforeBootstrap) {
        GWA3::Log::Info("Bootstrap: already in game before char-select hook; skipping RenderHook install");
    } else if (!GWA3::RenderHook::Initialize()) {
        GWA3::Log::Error("RenderHook failed - aborting");
        return 1;
    }
    if (!RunCharSelectBootstrap(90000, bootstrapCharacterName)) {
        GWA3::Log::Error("Bootstrap failed - aborting startup");
        return 1;
    }
    if (GWA3::RenderHook::IsInitialized()) {
        GWA3::RenderHook::SetMapLoaded(true);
    }
    WaitForPlayerHydration(45000);

    gameThreadOk = GWA3::GameThread::Initialize();

    GWA3::TraderHook::Initialize();
    GWA3::TargetLogHook::Initialize();
    GWA3::DialogHook::Initialize();

    if (!gameThreadOk) {
        GWA3::Log::Warn("GameThread initialization failed");
        if (!llmMode && !llmAdvisory) {
            GWA3::Log::Error("GameThread required for bot mode - aborting");
            return 1;
        }
    } else {
        GWA3::GameThread::Enqueue([]() {
            GWA3::Log::Info("Hello from game thread! Hook is working.");
        });
        GWA3::StoC::Initialize();
        GWA3::DialogMgr::Initialize();
        GWA3::ChatLogMgr::Initialize();
        GWA3::StringEncoding::Initialize();
        // EncStringCache installs a passive MinHook detour on
        // ValidateAsyncDecodeStr. That hook is currently unstable on the
        // Reforged client and has been the direct cause of advisory/LLM
        // runtime crashes during normal UI decode traffic. Nothing in the
        // live bridge currently requires this cache to function, so keep it
        // disabled by default until the hook path is repaired.
        if (llmMode || llmAdvisory) {
            GWA3::Log::Warn("EncStringCache disabled: ValidateAsyncDecodeStr hook is crash-prone on this client");
        }
    }

    if (llmAdvisory) {
        GWA3::Log::Info("=== LLM ADVISORY MODE (Froggy + LLM) ===");
        GWA3::RuntimeWatchdog::Start();
        GWA3::Bot::Froggy::Register();
        GWA3::Bot::Start();
        GWA3::Bot::SetState(IsInGame() ? GWA3::Bot::BotState::InTown
                                       : GWA3::Bot::BotState::CharSelect);
        if (!GWA3::LLM::Initialize()) {
            GWA3::Log::Error("LLM bridge initialization failed; Froggy running solo");
        } else {
            GWA3::Log::Info("gwa3.dll initialization complete - advisory mode active");
        }
        while (GWA3::Bot::IsRunning() || GWA3::LLM::IsRunning()) {
            Sleep(1000);
        }
        return 0;
    }

    if (llmMode) {
        GWA3::Log::Info("=== LLM AGENT MODE ===");
        GWA3::Bot::Froggy::Register();
        GWA3::Bot::SetState(IsInGame() ? GWA3::Bot::BotState::Idle
                                       : GWA3::Bot::BotState::CharSelect);
        if (!GWA3::LLM::Initialize()) {
            GWA3::Log::Error("LLM bridge initialization failed");
            return 1;
        }
        GWA3::Log::Info("gwa3.dll initialization complete - LLM bridge active");

        // Run everything on the init thread: snapshot serialization + action dispatch.
        // The bridge thread was causing game crashes when any game action was dispatched
        // while a pipe client was connected â€” even with EventPush disabled and snapshots
        // paused. Running all work on the init thread (proven safe) avoids the issue.
        // Run actions from init thread. Bridge thread handles snapshots normally.
        GWA3::Log::Info("[LLM] Init thread polling for actions");
        while (GWA3::LLM::IsRunning()) {
            GWA3::LLM::DrainInboundActions();
            Sleep(50);
        }
        return 0;
    }

    if (arachnisTest) {
        return RunArachnisHauntFlagMode();
    }

    if (ravensTest) {
        return RunRavensPointFlagMode();
    }

    if (rragarsTest) {
        return RunRragarsMenagerieFlagMode();
    }

    const auto selectedModule = ResolveSelectedBotModule();
    GWA3::Log::Info("Selected bot module: %s", GWA3::Bot::GetBotModuleName(selectedModule));
    GWA3::Bot::RegisterBotModule(selectedModule);
    GWA3::RuntimeWatchdog::Start();
    GWA3::Bot::Start();
    GWA3::Bot::SetState(IsInGame() ? GWA3::Bot::BotState::InTown
                                   : GWA3::Bot::BotState::CharSelect);

    GWA3::Log::Info("gwa3.dll initialization complete - bot started");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, &InitThread, hModule, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) {
        // During process termination, Windows may tear down CRT/static state
        // while we are still under the loader lock. Running full hook/thread/
        // STL cleanup here has been causing detach-time AVs inside gwa3.dll.
        // Let the OS reclaim our process resources instead.
        if (reserved != nullptr) {
            return TRUE;
        }
        GWA3::LLM::Shutdown();
        GWA3::Bot::Stop();
        GWA3::RuntimeWatchdog::Stop(false);
        GWA3::EncStringCache::Shutdown();
        GWA3::ChatLogMgr::Shutdown();
        GWA3::DialogMgr::Shutdown();
        GWA3::DialogHook::Shutdown();
        GWA3::StoC::Shutdown();
        GWA3::CtoSHook::Shutdown();
        GWA3::TraderHook::Shutdown();
        GWA3::TargetLogHook::Shutdown();
        GWA3::RenderHook::Shutdown();
        GWA3::GameThread::Shutdown();
        GWA3::CrashDiag::Shutdown();
        GWA3::Log::Shutdown();
    }
    return TRUE;
}
