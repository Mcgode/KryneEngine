cmake_minimum_required(VERSION 3.20)

add_executable(Core_Threads_UnitTests
        SpinLock_UnitTests.cpp
        LightweightSemaphore_UnitTests.cpp
        LightweightMutex_UnitTests.cpp
        Internal/FiberContext_UnitTests.cpp
        FibersManager_UnitTests.cpp)

target_link_libraries(Core_Threads_UnitTests KryneEngine_Core_Link TestUtils gtest gtest_main)
set_target_properties(Core_Threads_UnitTests PROPERTIES FOLDER "EngineTesting")

add_test(NAME Core_Threads_UnitTests COMMAND Core_Threads_UnitTests)
# Safety net: the FibersManager tests are regression tests for scheduler races that used to hang
# or deadlock; if one of them is ever reintroduced, fail the test (instead of hanging CI forever).
set_tests_properties(Core_Threads_UnitTests PROPERTIES TIMEOUT 60)