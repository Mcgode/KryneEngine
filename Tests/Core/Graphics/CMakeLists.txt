cmake_minimum_required(VERSION 3.20)

add_executable(Core_Graphics_UnitTests
        GraphicContext/Common.h
        GraphicContext/GraphicsContext_UnitTests.cpp
        GraphicContext/Buffer_UnitTests.cpp
        GraphicContext/Texture_UnitTests.cpp
        GraphicContext/ResourceCopy_UnitTests.cpp
)

target_link_libraries(Core_Graphics_UnitTests KryneEngine_Core TestUtils gtest gtest_main)
set_target_properties(Core_Graphics_UnitTests PROPERTIES FOLDER "EngineTesting")

add_test(NAME Core_Graphics_UnitTests COMMAND Core_Graphics_UnitTests)