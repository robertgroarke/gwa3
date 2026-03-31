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

# --- gwa3.dll ---
add_library(gwa3 SHARED
    src/dllmain.cpp
    src/core/Log.cpp
)

target_include_directories(gwa3 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gwa3 PRIVATE minhook)

set_target_properties(gwa3 PROPERTIES
    OUTPUT_NAME "gwa3"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# --- gwa3_tests.exe ---
add_executable(gwa3_tests
    tests/main.cpp
)

set_target_properties(gwa3_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Run tests as post-build step
add_custom_command(TARGET gwa3_tests POST_BUILD
    COMMAND $<TARGET_FILE:gwa3_tests>
    COMMENT "Running gwa3 offline tests..."
)
