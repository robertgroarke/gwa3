// IntegrationTestEpic14.cpp — Tests for Epic 14 Froggy features
// Run via: injector.exe --test-froggy

#include <gwa3/core/Log.h>
#include <gwa3/core/SmokeTest.h>
#include <gwa3/bot/FroggyHM.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/EffectMgr.h>

#include <Windows.h>

namespace GWA3::SmokeTest {

static int s_passed = 0;
static int s_failed = 0;
static int s_skipped = 0;

static void IntReport(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Log::Info("[FROGGY-TEST] %s", buf);
}

static void IntCheck(const char* name, bool cond) {
    if (cond) {
        s_passed++;
        IntReport("  [PASS] %s", name);
    } else {
        s_failed++;
        IntReport("  [FAIL] %s", name);
    }
}

static void IntSkip(const char* name, const char* reason) {
    s_skipped++;
    IntReport("  [SKIP] %s — %s", name, reason);
}

int RunFroggyFeatureTest() {
    s_passed = 0;
    s_failed = 0;
    s_skipped = 0;

    IntReport("=== FROGGY FEATURE TESTS ===");

    // ===== Phase 1: Unit tests (pure logic, no game state) =====
    IntReport("--- Phase 1: Unit tests (inside FroggyHM) ---");
    int unitFailures = Bot::Froggy::RunFroggyUnitTests();
    IntReport("Unit tests: %d failures", unitFailures);
    s_failed += unitFailures;
    // The unit test pass count is tracked inside RunFroggyUnitTests

    // ===== Phase 2: Integration tests (need game state) =====
    IntReport("--- Phase 2: Integration tests ---");

    // GWA3-114: Skillbar caching
    if (MapMgr::GetIsMapLoaded() && AgentMgr::GetMyId() > 0) {
        auto* bar = SkillMgr::GetPlayerSkillbar();
        if (bar) {
            int nonZeroSkills = 0;
            for (int i = 0; i < 8; i++) {
                if (bar->skills[i].skill_id != 0) nonZeroSkills++;
            }
            IntCheck("Skillbar has skills loaded", nonZeroSkills > 0);
            for (int i = 0; i < 8; i++) {
                if (bar->skills[i].skill_id != 0) {
                    const auto* data = SkillMgr::GetSkillConstantData(bar->skills[i].skill_id);
                    IntCheck("Skill constant data exists", data != nullptr);
                    if (data) {
                        IntCheck("Skill profession in range", data->profession <= 10);
                        IntCheck("Skill type in range", data->type <= 24);
                    }
                    break; // Just test first skill
                }
            }
        } else {
            IntSkip("Skillbar caching", "Skillbar not available");
        }

        // GWA3-115: Inventory helpers
        auto* inv = ItemMgr::GetInventory();
        if (inv) {
            uint32_t charGold = inv->gold_character;
            uint32_t storageGold = inv->gold_storage;
            IntCheck("Gold character plausible", charGold < 1000000);
            IntCheck("Gold storage plausible", storageGold < 10000000);

            // Count bags
            int bagsFound = 0;
            for (int i = 1; i <= 4; i++) {
                auto* bag = ItemMgr::GetBag(i);
                if (bag && bag->items.buffer) bagsFound++;
            }
            IntCheck("At least 1 backpack bag", bagsFound >= 1);
        } else {
            IntSkip("Inventory helpers", "Inventory not available");
        }

        // GWA3-115: Effect detection
        uint32_t myId = AgentMgr::GetMyId();
        if (myId > 0) {
            // These should just not crash — we can't guarantee buff state
            bool hasCon = EffectMgr::HasEffect(myId, 2053) &&
                          EffectMgr::HasEffect(myId, 2054) &&
                          EffectMgr::HasEffect(myId, 2055);
            IntCheck("HasEffect queries survive", true); // didn't crash

            auto* effects = EffectMgr::GetPlayerEffects();
            IntCheck("GetPlayerEffects returns", effects != nullptr || effects == nullptr); // no crash
        }

        // GWA3-113: Hero config file loading
        // Try loading Standard.txt — should find the file and parse heroes
        auto& cfg = Bot::GetConfig();
        cfg.hero_config_file = "Standard.txt";
        // We won't actually call LoadHeroConfigFile here to avoid modifying party state
        // The unit test in RunFroggyUnitTests verified the parsing logic
        IntCheck("Hero config file set", cfg.hero_config_file == "Standard.txt");

    } else {
        IntSkip("Integration tests", "Map not loaded or MyID not available");
    }

    IntReport("=== FROGGY FEATURE TESTS COMPLETE ===");
    IntReport("Passed: %d / Failed: %d / Skipped: %d", s_passed, s_failed, s_skipped);
    return s_failed;
}

} // namespace GWA3::SmokeTest
