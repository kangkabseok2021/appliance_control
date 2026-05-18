add_executable(test_fsm test_fsm.cpp)
target_link_libraries(test_fsm PRIVATE cycle_core GTest::gtest_main pthread)
include(GoogleTest)
gtest_discover_tests(test_fsm)
