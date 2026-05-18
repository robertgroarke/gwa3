#include <gwa3/testing/TestFramework.h>
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cmath>
#include <string>
#include <gwa3/game/Agent.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/Quest.h>
#include <gwa3/game/SkillIds.h>
#include <bots/common/BotFramework.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/game/Skill.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/packets/Headers.h>

#include "DungeonTestCoreStubs.h"
#include "DungeonTestInventoryQuestStubs.h"
#include "DungeonTestIoStubs.h"
#include "DungeonTestRuntimeStubs.h"
#include "dungeon_test_stubs_ext.cpp"

namespace GWA3::AdvancedEffects {
bool HasFullConset(uint32_t agentId) {
    return GWA3::EffectMgr::HasEffect(agentId, GWA3::SkillIds::ARMOR_OF_SALVATION_ITEM_EFFECT) &&
           GWA3::EffectMgr::HasEffect(agentId, GWA3::SkillIds::ESSENCE_OF_CELERITY_ITEM_EFFECT) &&
           GWA3::EffectMgr::HasEffect(agentId, GWA3::SkillIds::GRAIL_OF_MIGHT_ITEM_EFFECT);
}
} // namespace GWA3::AdvancedEffects

#include "test_dungeon_inventory_items_suite.cpp"

int main() {
    return GWA3::Testing::RunAll();
}
