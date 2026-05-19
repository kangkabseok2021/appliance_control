cmake_minimum_required(VERSION 3.21)
project(hil_controller CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build GoogleTest suite" ON)

find_package(Threads REQUIRED)

if(BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# Core HiL library
add_library(hil_core STATIC
  src/HilFsm.cpp
  src/MqttClient.cpp
)
target_include_directories(hil_core PUBLIC include third_party)
target_link_libraries(hil_core PUBLIC Threads::Threads)
target_compile_options(hil_core PRIVATE -Wall -Wextra)

# HiL controller binary
add_executable(hil_controller src/main_hil.cpp)
target_link_libraries(hil_controller PRIVATE hil_core)

# Tests (disabled for cross-compile targets — can't run ARM64 tests on x86)
if(BUILD_TESTS)
  enable_testing()
  add_executable(test_hil
    tests/test_fsm.cpp
    tests/test_mqtt_client.cpp
  )
  target_link_libraries(test_hil PRIVATE hil_core GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(test_hil)
endif()
