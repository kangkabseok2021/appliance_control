cmake_minimum_required(VERSION 3.21)
project(BatchMixerSCADA CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_definitions(_USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ── Core library (platform-independent) ─────────────────────────────────────
add_library(batch_mixer_lib STATIC
    src/Valve.cpp
    src/WeighingScale.cpp
    src/MixerDrum.cpp
    src/PressureSensor.cpp
    src/SimulatedAdsLink.cpp
    src/AdsLink.cpp
    src/PressureGuard.cpp
    src/BatchController.cpp
)
target_include_directories(batch_mixer_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
find_package(Threads REQUIRED)
target_link_libraries(batch_mixer_lib PUBLIC Threads::Threads)

# ── GoogleTest ───────────────────────────────────────────────────────────────
if(NOT TARGET GTest::gtest_main)
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

enable_testing()
add_executable(batch_mixer_tests
    tests/test_fsm.cpp
    tests/test_hal.cpp
)
target_link_libraries(batch_mixer_tests PRIVATE batch_mixer_lib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(batch_mixer_tests)

# ── Windows Service executable (Windows only) ────────────────────────────────
if(WIN32)
    add_executable(batch_mixer_service src/windows_service.cpp)
    target_link_libraries(batch_mixer_service PRIVATE batch_mixer_lib)
    target_compile_definitions(batch_mixer_service PRIVATE UNICODE _UNICODE)
endif()
