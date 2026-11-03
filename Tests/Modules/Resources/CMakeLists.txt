project(KryneEngine_Modules_Resources_Tests)

cmake_minimum_required(VERSION 3.31)

add_executable(Modules_Resources_UnitTests
        AsyncResourceLoader_UnitTests.cpp
)

target_link_libraries(Modules_Resources_UnitTests KryneEngine_Core_Link KryneEngine_Modules_Resources TestUtils gtest gtest_main)
set_target_properties(Modules_Resources_UnitTests PROPERTIES FOLDER "EngineTesting")

add_test(NAME Modules_Resources_UnitTests COMMAND Modules_Resources_UnitTests)
# Safety net: the loader runs its own threads, so a regression could hang instead of failing.
set_tests_properties(Modules_Resources_UnitTests PROPERTIES TIMEOUT 60)
