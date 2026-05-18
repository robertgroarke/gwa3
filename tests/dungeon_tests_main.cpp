#include <gwa3/testing/TestFramework.h>
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cmath>
#include <string>
#include <gwa3/game/Agent.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/Quest.h>
#include <bots/common/BotFramework.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/game/Skill.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/packets/Headers.h>

#include "DungeonTestCoreStubs.h"

#include "DungeonTestInventoryQuestStubs.h"

#include "DungeonTestIoStubs.h"

#include "DungeonTestRuntimeStubs.h"

int main() {
    return GWA3::Testing::RunAll();
}
