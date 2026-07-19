include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
set(gtest_force_shared_repo ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(sim_vision_tests
    test_parameter.cpp
    test_spsc_queue.cpp
    test_channel_manager.cpp
)
target_link_libraries(sim_vision_tests
    PRIVATE
        sim_vision_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(sim_vision_tests)
