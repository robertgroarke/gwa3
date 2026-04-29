cmake_minimum_required(VERSION 3.20)
project(gwa3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(GWA3_DLL_NAME "gwa3" CACHE STRING "Output DLL name (change for multi-agent builds)")
set(GWA3_PIPE_NAME "\\\\.\\pipe\\gwa3_llm" CACHE STRING "Named pipe used by the LLM bridge")

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
    GIT_TAG        master
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
    src/gwa3/dungeon/DungeonBuiltinCombat.cpp
    src/gwa3/dungeon/DungeonCombat.cpp
    src/gwa3/dungeon/DungeonCombatRoutine.cpp
    src/gwa3/dungeon/DungeonCheckpoint.cpp
    src/gwa3/dungeon/DungeonDiagnostics.cpp
    src/gwa3/dungeon/DungeonDialog.cpp
    src/gwa3/dungeon/DungeonEffects.cpp
    src/gwa3/dungeon/DungeonEntryRecovery.cpp
    src/gwa3/dungeon/DungeonInteractions.cpp
    src/gwa3/dungeon/DungeonInventory.cpp
    src/gwa3/dungeon/DungeonItemActions.cpp
    src/gwa3/dungeon/DungeonItemPolicy.cpp
    src/gwa3/dungeon/DungeonLoot.cpp
    src/gwa3/dungeon/DungeonOutpostSetup.cpp
    src/gwa3/dungeon/DungeonQuest.cpp
    src/gwa3/dungeon/DungeonQuestRuntime.cpp
    src/gwa3/dungeon/DungeonRouteRunner.cpp
    src/gwa3/dungeon/DungeonRuntime.cpp
    src/gwa3/dungeon/DungeonSkill.cpp
    src/gwa3/dungeon/DungeonVendor.cpp
    bots/frostmaws_burrows/FrostmawsBurrows.cpp
    bots/frostmaws_burrows/FrostmawsBurrowsBot.cpp
    bots/kathandrax/Kathandrax.cpp
    bots/kathandrax/KathandraxBot.cpp
    src/gwa3/dungeon/DungeonNavigation.cpp
    src/gwa3/dungeon/DungeonRoute.cpp
    src/gwa3/dungeon/DungeonTravel.cpp
    bots/ravens_point/RavensPoint.cpp
    bots/ravens_point/RavensPointBot.cpp
    bots/rragars_menagerie/RragarsMenagerie.cpp
    bots/rragars_menagerie/RragarsMenagerieBot.cpp
    bots/froggy/FroggyHM.cpp
    bots/froggy/FroggyHMRoutes.cpp
    src/gwa3/llm/IpcServer.cpp
    src/gwa3/llm/GameSnapshot.cpp
    src/gwa3/llm/ActionExecutor.cpp
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

