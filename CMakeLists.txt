cmake_minimum_required(VERSION 3.20)
project(gwa3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# --- gwa3_core (OBJECT library — shared between DLL and tests) ---
add_library(gwa3_core OBJECT
    src/core/Log.cpp
    src/core/Scanner.cpp
    src/core/Offsets.cpp
    src/core/GameThread.cpp
    src/core/Memory.cpp
    src/core/RenderHook.cpp
    src/core/TraderHook.cpp
    src/core/TargetLogHook.cpp
    src/core/CallbackRegistry.cpp
    src/packets/CtoS.cpp
    src/managers/AgentMgr.cpp
    src/managers/SkillMgr.cpp
    src/managers/ItemMgr.cpp
    src/managers/MapMgr.cpp
    src/managers/PartyMgr.cpp
    src/managers/QuestMgr.cpp
    src/managers/MemoryMgr.cpp
    src/managers/CameraMgr.cpp
    src/managers/GuildMgr.cpp
    src/managers/UIMgr.cpp
    src/managers/ChatMgr.cpp
    src/managers/TradeMgr.cpp
    src/managers/FriendListMgr.cpp
    src/managers/PlayerMgr.cpp
    src/managers/StoCMgr.cpp
    src/managers/EffectMgr.cpp
    src/managers/DialogMgr.cpp
    src/utils/EncStrCodec.cpp
    src/bot/BotFramework.cpp
    src/bot/FroggyHM.cpp
    src/tests/SmokeTest.cpp
    src/tests/BotFrameworkTest.cpp
    src/tests/BehavioralTest.cpp
    src/tests/IntegrationTest.cpp
    src/llm/IpcServer.cpp
    src/llm/GameSnapshot.cpp
    src/llm/ActionExecutor.cpp
    src/llm/LlmBridge.cpp
)

target_include_directories(gwa3_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Crash investigation test selector (0=normal, 1=remove JMP after bootstrap, 2=disable GameThread)
set(CRASH_TEST 1 CACHE STRING "Crash test mode")
target_compile_definitions(gwa3_core PUBLIC CRASH_TEST=${CRASH_TEST})

# gwa3_core needs MinHook headers (for GameThread.cpp), version.lib (for MemoryMgr),
# and nlohmann_json (for LLM bridge serialization)
target_link_libraries(gwa3_core PUBLIC minhook version nlohmann_json::nlohmann_json)

# --- gwa3.dll ---
add_library(gwa3 SHARED
    src/dllmain.cpp
    src/utils/StringEncoding.cpp
    $<TARGET_OBJECTS:gwa3_core>
)

target_include_directories(gwa3 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(gwa3 PRIVATE CRASH_TEST=${CRASH_TEST})
target_link_libraries(gwa3 PRIVATE minhook version)

set_target_properties(gwa3 PROPERTIES
    OUTPUT_NAME "gwa3"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# --- gwa3_tests.exe ---
add_executable(gwa3_tests
    tests/main.cpp
    $<TARGET_OBJECTS:gwa3_core>
)

target_include_directories(gwa3_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gwa3_tests PRIVATE minhook version)

set_target_properties(gwa3_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
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

# Run tests as post-build step
add_custom_command(TARGET gwa3_tests POST_BUILD
    COMMAND $<TARGET_FILE:gwa3_tests>
    COMMENT "Running gwa3 offline tests..."
)
