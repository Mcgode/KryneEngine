
# Compiler flags

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-trace")
    if (WIN32)
        # Asynchronous (SEH-aware) exceptions. EAThread's Windows backend relies on
        # __try/__except, so C++ frames must be unwindable through structured
        # exceptions. The spelling differs between the clang-cl and the GNU driver
        # (the latter is what the macOS -> mingw cross-compile toolchain uses).
        if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHa")
        else ()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fasync-exceptions")
        endif ()
    endif ()
endif()

# Parse build type
string(REPLACE " " ";" BUILD_TYPE ${CMAKE_BUILD_TYPE})

list(LENGTH BUILD_TYPE TOTAL_BUILD_ARGS_LENGTH)

if (${TOTAL_BUILD_ARGS_LENGTH} GREATER_EQUAL 2)
    list(GET BUILD_TYPE 1 Arg)

    if (Arg STREQUAL "DX12")
        set(GraphicsApi "DX12")
    endif()

    if (Arg STREQUAL "MTL")
        set(GraphicsApi "MTL")
        enable_language(OBJC OBJCXX)
    endif()
endif()

if (NOT DEFINED GraphicsApi)
    set(GraphicsApi "VK")
endif()

message(STATUS "Using API: " ${GraphicsApi})

list(GET BUILD_TYPE 0 TypeName)
set(CMAKE_BUILD_TYPE "${TypeName}")

message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})

# Coverage instrumentation needs LLVM's source-based coverage (Clang/AppleClang only), a
# compiler-rt profile runtime that's actually shipped for the target, and a RunCoverage
# script to drive it (only written for Windows, macOS and Linux so far).
set(KRYNE_ENGINE_COVERAGE_SUPPORTED FALSE)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CROSSCOMPILING AND (WIN32 OR APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    set(KRYNE_ENGINE_COVERAGE_SUPPORTED TRUE)
endif ()

function(AddCoverage TargetName)
    if (KRYNE_ENGINE_ENABLE_COVERAGE AND KRYNE_ENGINE_COVERAGE_SUPPORTED)
        target_compile_options(${TargetName} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        # PUBLIC (rather than PRIVATE): for a STATIC library, CMake ignores non-interface
        # link options, so this must propagate as an interface option to reach the final
        # link step of whatever executable consumes this target (e.g. the test binaries).
        target_link_options(${TargetName} PUBLIC -fprofile-instr-generate)
    endif()
endfunction()

function(CopyDLLs TargetName)
    add_custom_command(TARGET ${TargetName} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
            -DDLLs=$<TARGET_RUNTIME_DLLS:${TargetName}>
            -DOUT_DIR=$<TARGET_FILE_DIR:${TargetName}>
            -P "${CMAKE_SOURCE_DIR}/CMake/CopyRuntimeDeps.cmake"
            VERBATIM
    )
endfunction()