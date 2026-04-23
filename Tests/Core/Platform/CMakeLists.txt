cmake_minimum_required(VERSION 3.20)

add_executable(Core_Platform_UnitTests
        Cpu_UnitTests.cpp
)

target_link_libraries(Core_Platform_UnitTests KryneEngine_Core_Link TestUtils gtest_main)
set_target_properties(Core_Platform_UnitTests PROPERTIES FOLDER "EngineTesting")

add_test(NAME Core_Platform_UnitTests COMMAND Core_Memory_UnitTests)
