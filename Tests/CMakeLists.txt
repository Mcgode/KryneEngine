add_library(TestUtils
        Utils/AssertUtils.hpp
        Utils/AssertUtils.cpp
        Utils/Comparison.hpp
        Utils/SvgDump.cpp
        Utils/SvgDump.hpp)

target_include_directories(TestUtils PUBLIC .)

target_link_libraries(TestUtils KryneEngine_Core gtest)
set_target_properties(TestUtils PROPERTIES FOLDER "EngineTesting")

add_subdirectory(Core)
add_subdirectory(Modules)