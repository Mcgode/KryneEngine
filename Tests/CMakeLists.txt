add_library(TestUtils
        Utils/AssertUtils.hpp
        Utils/AssertUtils.cpp
        Utils/Comparison.hpp
        Utils/SvgDump.cpp
        Utils/SvgDump.hpp)

target_include_directories(TestUtils PUBLIC .)

target_link_libraries(TestUtils KryneEngine_Core_Link gtest)
set_target_properties(TestUtils PROPERTIES FOLDER "EngineTesting")

add_subdirectory(Core)
add_subdirectory(Modules)

if (KRYNE_ENGINE_ENABLE_COVERAGE AND KRYNE_ENGINE_COVERAGE_SUPPORTED)
    if (WIN32)
        add_custom_target(coverage
                COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                        -File "${CMAKE_SOURCE_DIR}/CMake/RunCoverage.ps1"
                        -BuildDir "${CMAKE_BINARY_DIR}" -Compiler "${CMAKE_CXX_COMPILER}"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                USES_TERMINAL
                COMMENT "Running unit tests and generating a code coverage report")
    else ()
        add_custom_target(coverage
                COMMAND bash "${CMAKE_SOURCE_DIR}/CMake/RunCoverage.sh" "${CMAKE_BINARY_DIR}" "${CMAKE_CXX_COMPILER}"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                USES_TERMINAL
                COMMENT "Running unit tests and generating a code coverage report")
    endif ()
endif ()