cmake_minimum_required(VERSION 3.21)
project(appliance_control CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Core library (no main) — shared by controller binary and tests
add_library(cycle_core STATIC
  src/CycleController.cpp
  src/SimulatedComponents.cpp
)
target_include_directories(cycle_core PUBLIC include third_party)
target_compile_options(cycle_core PRIVATE -Wall -Wextra)

# Controller binary
add_executable(appliance_controller src/main.cpp)
target_link_libraries(appliance_controller PRIVATE cycle_core pthread)

# Test binary
enable_testing()
add_subdirectory(tests)
