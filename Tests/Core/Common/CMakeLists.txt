cmake_minimum_required(VERSION 3.20)

add_executable(Core_Common_UnitTests
        Assert_UnitTests.cpp
        Utils/Alignment_UnitTests.cpp)

target_link_libraries(Core_Common_UnitTests KryneEngine_Core TestUtils gtest gtest_main)
set_target_properties(Core_Common_UnitTests PROPERTIES FOLDER "EngineTesting")

add_test(NAME Core_Common_UnitTests COMMAND Core_Common_UnitTests)