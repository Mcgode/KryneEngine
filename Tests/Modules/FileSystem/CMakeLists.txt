project(KryneEngine_Modules_GraphicsUtils_Tests)

cmake_minimum_required(VERSION 3.31)

add_executable(Modules_FileSystem_UnitTests
        Utils_UnitTests.cpp
        DirectoryTree_UnitTests.cpp
)

target_link_libraries(Modules_FileSystem_UnitTests KryneEngine_Core_Link KryneEngine_Modules_FileSystem TestUtils gtest gtest_main)
set_target_properties(Modules_FileSystem_UnitTests PROPERTIES FOLDER "EngineTesting")

add_test(NAME Modules_FileSystem_UnitTests COMMAND Modules_FileSystem_UnitTests)