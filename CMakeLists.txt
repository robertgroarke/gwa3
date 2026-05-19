cmake_minimum_required(VERSION 3.20)
project(gwa3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(GWA3_DLL_NAME "gwa3" CACHE STRING "Output DLL name (change for multi-agent builds)")
set(GWA3_PIPE_NAME "\\\\.\\pipe\\gwa3_llm" CACHE STRING "Named pipe used by the LLM bridge")
option(GWA3_ENABLE_INJECTED_TESTS "Compile injected SmokeTest/IntegrationTest modes into the DLL" OFF)

# --- MSVC flags ---
if(MSVC)
    # /W4 warning level, /MT static runtime (no MSVC redist dependency)
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    add_compile_options(/W4)
endif()

# --- FetchContent: MinHook ---
include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.4
)
FetchContent_MakeAvailable(minhook)

# --- FetchContent: nlohmann/json (header-only) ---
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)
if(NOT TARGET nlohmann_json::nlohmann_json)
    set(JSON_INCLUDE_ROOT "")
    if(DEFINED json_SOURCE_DIR AND EXISTS "${json_SOURCE_DIR}/include")
        set(JSON_INCLUDE_ROOT "${json_SOURCE_DIR}/include")
    elseif(DEFINED FETCHCONTENT_SOURCE_DIR_JSON AND EXISTS "${FETCHCONTENT_SOURCE_DIR_JSON}/include")
        set(JSON_INCLUDE_ROOT "${FETCHCONTENT_SOURCE_DIR_JSON}/include")
    endif()
    if(NOT JSON_INCLUDE_ROOT STREQUAL "")
        add_library(nlohmann_json INTERFACE)
        target_include_directories(nlohmann_json INTERFACE "${JSON_INCLUDE_ROOT}")
        add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
    else()
        message(FATAL_ERROR "nlohmann_json target missing and no json include directory was found")
    endif()
endif()

# --- gwa3_core (OBJECT library shared by the DLL and tools) ---
add_library(gwa3_core OBJECT
    src/gwa3/core/Log.cpp
    src/gwa3/core/Scanner.cpp
    src/gwa3/core/Offsets.cpp
    src/gwa3/core/GameThread.cpp
    src/gwa3/core/Memory.cpp
    src/gwa3/core/DialogHook.cpp
    src/gwa3/core/CrashDiag.cpp
    src/gwa3/core/HookMarker.cpp
    src/gwa3/core/RuntimeWatchdog.cpp
    src/gwa3/core/RenderHook.cpp
    src/gwa3/core/TraderHook.cpp
    src/gwa3/core/TradePartnerHook.cpp
    src/gwa3/core/TargetLogHook.cpp
    src/gwa3/core/CallbackRegistry.cpp
    src/gwa3/packets/CtoS.cpp
    src/gwa3/packets/CtoSHook.cpp
    src/gwa3/managers/AgentMgr.cpp
    src/gwa3/managers/SkillMgr.cpp
    src/gwa3/managers/ItemMgr.cpp
    src/gwa3/managers/MapMgr.cpp
    src/gwa3/managers/PartyMgr.cpp
    src/gwa3/managers/QuestMgr.cpp
    src/gwa3/managers/MemoryMgr.cpp
    src/gwa3/managers/CameraMgr.cpp
    src/gwa3/managers/GuildMgr.cpp
    src/gwa3/managers/UIMgr.cpp
    src/gwa3/managers/ChatMgr.cpp
    src/gwa3/managers/TradeMgr.cpp
    src/gwa3/managers/MerchantMgr.cpp
    src/gwa3/managers/MaintenanceMgr.cpp
    src/gwa3/managers/FriendListMgr.cpp
    src/gwa3/managers/PlayerMgr.cpp
    src/gwa3/managers/StoCMgr.cpp
    src/gwa3/managers/EffectMgr.cpp
    src/gwa3/managers/DialogMgr.cpp
    src/gwa3/managers/ChatLogMgr.cpp
    src/gwa3/utils/EncStrCodec.cpp
    src/gwa3/utils/EncStringCache.cpp
    bots/common/BotFramework.cpp
    bots/common/DungeonBotStates.cpp
    bots/arachnis_haunt/ArachnisHaunt.cpp
    bots/arachnis_haunt/ArachnisHauntBot.cpp
    bots/common/BotModuleRegistry.cpp
    bots/common/BotModuleSelector.cpp
    src/gwa3/dungeon/DungeonBundle.cpp
    src/gwa3/managers/CombatMgr.cpp
    src/gwa3/dungeon/DungeonCombat.cpp
    src/gwa3/advanced/Combat.cpp
    src/gwa3/advanced/CombatRoutine.cpp
    src/gwa3/dungeon/DungeonCheckpoint.cpp
    src/gwa3/advanced/Diagnostics.cpp
    src/gwa3/advanced/Dialog.cpp
    src/gwa3/advanced/Effects.cpp
    src/gwa3/dungeon/DungeonEntryRecovery.cpp
    src/gwa3/advanced/Interactions.cpp
    src/gwa3/advanced/Inventory.cpp
    src/gwa3/advanced/ItemActions.cpp
    src/gwa3/advanced/ItemPolicy.cpp
    src/gwa3/advanced/Loot.cpp
    src/gwa3/advanced/Waypoint.cpp
    src/gwa3/dungeon/DungeonLoot.cpp
    src/gwa3/dungeon/DungeonRunStats.cpp
    src/gwa3/advanced/OutpostSetup.cpp
    src/gwa3/advanced/QuestPlan.cpp
    src/gwa3/dungeon/DungeonQuestRuntime.cpp
    src/gwa3/dungeon/DungeonRouteRunner.cpp
    src/gwa3/advanced/Runtime.cpp
    src/gwa3/managers/SkillCombat.cpp
    src/gwa3/managers/MerchantServices.cpp
    bots/frostmaws_burrows/FrostmawsBurrows.cpp
    bots/frostmaws_burrows/FrostmawsBurrowsBot.cpp
    bots/kathandrax/Kathandrax.cpp
    bots/kathandrax/KathandraxBot.cpp
    src/gwa3/dungeon/DungeonNavigation.cpp
    src/gwa3/dungeon/DungeonRoute.cpp
    src/gwa3/managers/TravelMgr.cpp
    bots/ravens_point/RavensPoint.cpp
    bots/ravens_point/RavensPointBot.cpp
    bots/rragars_menagerie/RragarsMenagerie.cpp
    bots/rragars_menagerie/RragarsMenagerieBot.cpp
    bots/froggy/FroggyHM.cpp
    src/gwa3/llm/IpcServer.cpp
    src/gwa3/llm/GameSnapshot.cpp
    src/gwa3/llm/RouteWalker.cpp
    src/gwa3/llm/ActionExecutor.cpp
    src/gwa3/llm/ActionExecutorCombat.cpp
    src/gwa3/llm/ActionExecutorInteraction.cpp
    src/gwa3/llm/ActionExecutorMovement.cpp
    src/gwa3/llm/ActionExecutorUtility.cpp
    src/gwa3/llm/EventPush.cpp
    src/gwa3/llm/LlmBridge.cpp
)

target_include_directories(gwa3_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

string(REPLACE "\\" "\\\\" GWA3_PIPE_NAME_ESCAPED "${GWA3_PIPE_NAME}")
target_compile_definitions(gwa3_core PUBLIC GWA3_PIPE_NAME="${GWA3_PIPE_NAME_ESCAPED}")

# gwa3_core needs MinHook headers (for GameThread.cpp), version.lib (for MemoryMgr),
# and nlohmann_json (for LLM bridge serialization)
target_link_libraries(gwa3_core PUBLIC minhook version Dbghelp.lib nlohmann_json::nlohmann_json)

# --- gwa3.dll ---
add_library(gwa3 SHARED
    src/gwa3/dllmain.cpp
    src/gwa3/utils/StringEncoding.cpp
    $<TARGET_OBJECTS:gwa3_core>
)
set_target_properties(gwa3 PROPERTIES OUTPUT_NAME "${GWA3_DLL_NAME}")

target_include_directories(gwa3 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gwa3 PRIVATE minhook version Dbghelp.lib)

set_target_properties(gwa3 PROPERTIES
    OUTPUT_NAME "${GWA3_DLL_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LINK_FLAGS "/MAP"
)

# --- injector.exe ---
add_executable(injector
    tools/injector.cpp
)

set_target_properties(injector PROPERTIES
    OUTPUT_NAME "injector"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# --- pattern_test.exe ---
add_executable(pattern_test
    tools/pattern_test.cpp
)

set_target_properties(pattern_test PROPERTIES
    OUTPUT_NAME "pattern_test"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(gwa3_tests
    tests/offline_inventory.cpp
    src/gwa3/advanced/Inventory.cpp
    src/gwa3/advanced/ItemActions.cpp
    src/gwa3/advanced/ItemPolicy.cpp
)

target_include_directories(gwa3_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gwa3_tests PRIVATE nlohmann_json::nlohmann_json)

set_target_properties(gwa3_tests PROPERTIES
    OUTPUT_NAME "gwa3_tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

enable_testing()
add_test(NAME gwa3_tests COMMAND gwa3_tests)

# --- gwa3_dungeon_tests.exe ---
add_executable(gwa3_dungeon_tests
    tests/dungeon_tests_main.cpp
    tests/dungeon_test_stubs_ext.cpp
    tests/DungeonCombatTestSupport.cpp
    tests/DungeonNavigationTestSupport.cpp
    tests/DungeonSupportRuntime.cpp
    tests/test_arachnis_suite.cpp
    tests/test_bot_misc_suite.cpp
    tests/test_core_lowlevel_suite.cpp
    tests/test_dungeon_bundle_suite.cpp
    tests/test_dungeon_checkpoint_suite.cpp
    tests/test_dungeon_combat_advance_suite.cpp
    tests/test_dungeon_combat_clear_builtin_suite.cpp
    tests/test_dungeon_combat_follow_suite.cpp
    tests/test_dungeon_combat_routine_suite.cpp
    tests/test_dungeon_dialog_effects_quest_suite.cpp
    tests/test_dungeon_interactions_suite.cpp
    tests/test_dungeon_inventory_items_suite.cpp
    tests/test_dungeon_loot_suite.cpp
    tests/test_dungeon_navigation_runtime_suite.cpp
    tests/test_dungeon_outpost_quest_runtime_a_suite.cpp
    tests/test_dungeon_quest_runtime_b_suite.cpp
    tests/test_dungeon_route_travel_vendor_suite.cpp
    tests/test_froggy_config_tuning.cpp
    tests/test_froggy_suite.cpp
    tests/test_frostmaws_suite.cpp
    tests/test_headers.cpp
    tests/test_kathandrax_suite.cpp
    tests/test_ravens_suite.cpp
    tests/test_rragars_suite.cpp
    tests/test_struct_offsets_agents_items.cpp
    tests/test_struct_offsets_world.cpp
    src/gwa3/core/Scanner.cpp
    src/gwa3/utils/EncStrCodec.cpp
    src/gwa3/utils/StringEncoding.cpp
    bots/common/BotModuleRegistry.cpp
    bots/common/BotModuleSelector.cpp
    bots/common/DungeonBotStates.cpp
    bots/arachnis_haunt/ArachnisHaunt.cpp
    bots/arachnis_haunt/ArachnisHauntBot.cpp
    bots/froggy/FroggyHM.cpp
    bots/frostmaws_burrows/FrostmawsBurrows.cpp
    bots/frostmaws_burrows/FrostmawsBurrowsBot.cpp
    bots/kathandrax/Kathandrax.cpp
    bots/kathandrax/KathandraxBot.cpp
    bots/ravens_point/RavensPoint.cpp
    bots/ravens_point/RavensPointBot.cpp
    bots/rragars_menagerie/RragarsMenagerie.cpp
    bots/rragars_menagerie/RragarsMenagerieBot.cpp
    src/gwa3/advanced/Combat.cpp
    src/gwa3/advanced/CombatRoutine.cpp
    src/gwa3/advanced/Diagnostics.cpp
    src/gwa3/advanced/Dialog.cpp
    src/gwa3/advanced/Effects.cpp
    src/gwa3/advanced/Interactions.cpp
    src/gwa3/advanced/Inventory.cpp
    src/gwa3/advanced/ItemActions.cpp
    src/gwa3/advanced/ItemPolicy.cpp
    src/gwa3/advanced/Loot.cpp
    src/gwa3/advanced/OutpostSetup.cpp
    src/gwa3/advanced/QuestPlan.cpp
    src/gwa3/advanced/Runtime.cpp
    src/gwa3/advanced/Waypoint.cpp
    src/gwa3/dungeon/DungeonBundle.cpp
    src/gwa3/dungeon/DungeonCheckpoint.cpp
    src/gwa3/dungeon/DungeonCombat.cpp
    src/gwa3/dungeon/DungeonEntryRecovery.cpp
    src/gwa3/dungeon/DungeonLoot.cpp
    src/gwa3/dungeon/DungeonNavigation.cpp
    src/gwa3/dungeon/DungeonQuestRuntime.cpp
    src/gwa3/dungeon/DungeonRoute.cpp
    src/gwa3/dungeon/DungeonRouteRunner.cpp
    src/gwa3/dungeon/DungeonRunStats.cpp
    src/gwa3/managers/CombatMgr.cpp
    src/gwa3/managers/MerchantServices.cpp
    src/gwa3/managers/SkillCombat.cpp
    src/gwa3/managers/TravelMgr.cpp
)

target_include_directories(gwa3_dungeon_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_compile_definitions(gwa3_dungeon_tests PRIVATE GWA3_DUNGEON_TEST_BUILD=1)
target_link_libraries(gwa3_dungeon_tests PRIVATE minhook version Dbghelp.lib nlohmann_json::nlohmann_json)

if(MSVC)
    target_compile_options(gwa3_dungeon_tests PRIVATE
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/tests/DungeonTestCommon.h"
    )
else()
    target_compile_options(gwa3_dungeon_tests PRIVATE
        "-include" "${CMAKE_CURRENT_SOURCE_DIR}/tests/DungeonTestCommon.h"
    )
endif()

set_target_properties(gwa3_dungeon_tests PROPERTIES
    OUTPUT_NAME "gwa3_dungeon_tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME gwa3_dungeon_tests COMMAND gwa3_dungeon_tests)
