cmake_minimum_required(VERSION 3.20)

add_executable(Core_Memory_UnitTests
        DynamicArray_UnitTests.cpp
        GenerationalPool_UnitTests.cpp
        TlsfAllocator_UnitTests.cpp)

target_link_libraries(Core_Memory_UnitTests KryneEngine_Core TestUtils gtest gtest_main)
set_target_properties(Core_Memory_UnitTests PROPERTIES FOLDER "EngineTesting")

add_test(NAME Core_Memory_UnitTests COMMAND Core_Memory_UnitTests)