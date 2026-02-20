
# Compiler flags

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHa -ftime-trace")
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

function(AddCoverage TargetName)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${TargetName} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
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