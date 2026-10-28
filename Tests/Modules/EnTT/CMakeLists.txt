project(KryneEngine_Modules_EnTT_Tests)

cmake_minimum_required(VERSION 3.31)

add_executable(Modules_EnTT_UnitTests
        Context_UnitTests.cpp
)

target_link_libraries(Modules_EnTT_UnitTests KryneEngine_Core_Link KryneEngine_Modules_EnTT TestUtils gtest gtest_main)
set_target_properties(Modules_EnTT_UnitTests PROPERTIES FOLDER "EngineTesting")

add_test(NAME Modules_EnTT_UnitTests COMMAND Modules_EnTT_UnitTests)
