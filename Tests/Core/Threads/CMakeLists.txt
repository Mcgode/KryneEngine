cmake_minimum_required(VERSION 3.20)

add_executable(Core_Threads_UnitTests
        SpinLock_UnitTests.cpp
        LightweightSemaphore_UnitTests.cpp
        LightweightMutex_UnitTests.cpp
        Internal/FiberContext_UnitTests.cpp)

target_link_libraries(Core_Threads_UnitTests KryneEngine_Core TestUtils gtest gtest_main)
set_target_properties(Core_Threads_UnitTests PROPERTIES FOLDER "EngineTesting")

add_test(NAME Core_Threads_UnitTests COMMAND Core_Threads_UnitTests)