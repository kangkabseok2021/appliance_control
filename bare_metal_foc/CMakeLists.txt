cmake_minimum_required(VERSION 3.21)
project(BaremetalFOC C)

# Bare-metal: C11 (no C++ — ANSI C for MCU compliance)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Zero-heap enforcement: -Wvla -Walloca
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wvla -Walloca -Wno-unused-parameter)
endif()
if(MSVC)
    add_compile_options(/W3)
endif()

# ── Core FOC library (no heap, no math.h) ────────────────────────────────────
add_library(foc_core STATIC
    src/transforms.c
    src/pid.c
    src/can_frame.c
    src/can_sim.c
)
target_include_directories(foc_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_definitions(foc_core PRIVATE CAN_SIMULATION)

# ── Unity test framework (MIT, fetched from ThrowTheSwitch) ──────────────────
include(FetchContent)
FetchContent_Declare(unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.5.2
    GIT_SHALLOW    TRUE
)
set(UNITY_EXTENSION_FLOAT ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(unity)

# ── Test runner binary ────────────────────────────────────────────────────────
enable_testing()
add_executable(foc_tests
    tests/test_runner.c
    tests/test_clarke.c
    tests/test_park.c
    tests/test_pid.c
    tests/test_can.c
    tests/test_integration.c
)
target_include_directories(foc_tests PRIVATE include)
target_link_libraries(foc_tests PRIVATE foc_core unity m)
target_compile_definitions(foc_tests PRIVATE CAN_SIMULATION)

add_test(NAME foc_unity_tests COMMAND foc_tests)
