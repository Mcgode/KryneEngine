
# Fetch the Slang compiler executable.
# Shaders are compiled at build time, so slangc must be a *host* executable - pick it
# from CMAKE_HOST_* rather than the (possibly cross-compilation) target.
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(PLATFORM "macos")
    set(SLANGC_NAME "slangc")
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM "windows")
    set(SLANGC_NAME "slangc.exe")
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM "linux")
    set(SLANGC_NAME "slangc")
else ()
    message(FATAL_ERROR "Host platform unsupported for shader compilation: ${CMAKE_HOST_SYSTEM_NAME}")
endif ()

string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" SYSTEM_PROCESSOR)
if (SYSTEM_PROCESSOR MATCHES "amd64|x64|x86_64")
    set(ARCH "x64")
elseif (SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(ARCH "aarch64")
endif ()

# Slang emits DXIL through the dxcompiler library, which it does not bundle: point it at ours.
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(DxcLibraryDir "${CMAKE_CURRENT_SOURCE_DIR}/External/DirectXCompiler/macos/lib")
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    if (ARCH STREQUAL "aarch64")
        set(DxcLibraryDir "${CMAKE_CURRENT_SOURCE_DIR}/External/DirectXCompiler/win32/bin/arm64")
    else ()
        set(DxcLibraryDir "${CMAKE_CURRENT_SOURCE_DIR}/External/DirectXCompiler/win32/bin/x64")
    endif ()
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    if (ARCH STREQUAL "aarch64")
        set(DxcLibraryDir "${CMAKE_CURRENT_SOURCE_DIR}/External/DirectXCompiler/linux/arm64/lib")
    else ()
        set(DxcLibraryDir "${CMAKE_CURRENT_SOURCE_DIR}/External/DirectXCompiler/linux/x64/lib")
    endif ()
endif ()

set(SlangCompiler "${CMAKE_CURRENT_SOURCE_DIR}/External/slang/${PLATFORM}/${ARCH}/bin/${SLANGC_NAME}")
option(KRYNE_ENGINE_OPTIMIZE_SPIRV "Enable Slang SPIR-V optimization" OFF)

# Check that we found the executable
if (EXISTS "${SlangCompiler}")
    message(STATUS "Will compile shaders using '${SlangCompiler}'")
else ()
    message(FATAL_ERROR "Unable to find '${SlangCompiler}'")
endif ()

# Global variables set
set_property(GLOBAL PROPERTY KE_SHADER_SLANGC "${SlangCompiler}")
set_property(GLOBAL PROPERTY KE_SHADER_DXC_LIBRARY_DIR "${DxcLibraryDir}")
set_property(GLOBAL PROPERTY KE_SHADER_GENERATE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/CMake/ShaderListParser.py")
set_property(GLOBAL PROPERTY KE_SHADER_EMBED_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/CMake/ShaderEmbedGenerator.py")
find_package(Python3 REQUIRED)
# Python3_EXECUTABLE is only visible in this directory scope, store it globally for projects consuming the engine
set_property(GLOBAL PROPERTY KE_SHADER_PYTHON "${Python3_EXECUTABLE}")

# target_compile_shaders implementation
# Pass the EMBED option to also generate a <TARGET_NAME>_EmbeddedShaders.cpp/.h pair (added to the
# target's sources) exposing each compiled shader as a `eastl::span<const u8>` accessor function,
# under the `KryneEngine::EmbeddedShaders::<TARGET_NAME>` namespace.
function(target_compile_shaders TARGET_NAME LOCAL_SHADERS_DIR OUTPUT_DIR_NAME)
    cmake_parse_arguments(ARG "EMBED" "" "" ${ARGN})

    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Shaders")
    set(SHADER_BUILD_OUTPUT_DIR "${CMAKE_BINARY_DIR}/ShaderBuild")

    file(RELATIVE_PATH TARGET_REL_PATH ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

    set(SHADER_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${LOCAL_SHADERS_DIR}")
    set(BUILD_OUTPUT_DIR "${SHADER_BUILD_OUTPUT_DIR}/${OUTPUT_DIR_NAME}")

    if (GraphicsApi STREQUAL "VK")
        set(OutputFormat "spirv")
    elseif (GraphicsApi STREQUAL "DX12")
        set(OutputFormat "cso")
    elseif (GraphicsApi STREQUAL "MTL")
        set(OutputFormat "metallib")
    endif()

    if (NOT ${CMAKE_GENERATOR} MATCHES "Ninja")
        message(FATAL_ERROR "System currently exclusively supports Ninja")
    endif ()
    message(STATUS "${CMAKE_MAKE_PROGRAM}")

    file(GLOB_RECURSE ShaderListFiles "${SHADER_INPUT_DIR}" "*.shader")
    message(STATUS "Shader list files list: ${ShaderListFiles}")

    get_target_property(SHADER_INCLUDE_LIST ${TARGET_NAME} SHADER_INCLUDE_LIST)
    if (SHADER_INCLUDE_LIST STREQUAL "SHADER_INCLUDE_LIST-NOTFOUND")
        set(SHADER_INCLUDE_LIST "None")
    endif ()
    list(LENGTH SHADER_INCLUDE_LIST LIST_COUNT)
    if (LIST_COUNT EQUAL 0)
        set(SHADER_INCLUDE_LIST "None")
    endif ()

    # Flatten the list into a single argument: a raw ';' would be interpreted as a command
    # separator by the shell when more than one shader library is linked.
    string(REPLACE ";" "%%" SHADER_INCLUDE_LIST "${SHADER_INCLUDE_LIST}")

    set(COMMANDS_FILE "${BUILD_OUTPUT_DIR}/build.ninja")
    set(WORKING_DIR ${BUILD_OUTPUT_DIR})

    get_property(SlangCompiler GLOBAL PROPERTY KE_SHADER_SLANGC)
    get_property(DxcLibraryDir GLOBAL PROPERTY KE_SHADER_DXC_LIBRARY_DIR)
    get_property(GENERATE_SCRIPT GLOBAL PROPERTY KE_SHADER_GENERATE_SCRIPT)
    get_property(EMBED_SCRIPT GLOBAL PROPERTY KE_SHADER_EMBED_SCRIPT)
    get_property(PYTHON_EXECUTABLE GLOBAL PROPERTY KE_SHADER_PYTHON)

    set(EMBED_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Embedded/${TARGET_REL_PATH}")
    if (ARG_EMBED)
        set(EMBED_ENABLED "1")
        set(EMBED_CPP "${EMBED_OUTPUT_DIR}/${TARGET_NAME}_EmbeddedShaders.cpp")
        set(EMBED_H "${EMBED_OUTPUT_DIR}/${TARGET_NAME}_EmbeddedShaders.h")
    else ()
        set(EMBED_ENABLED "0")
        set(EMBED_CPP "${EMBED_OUTPUT_DIR}/unused.cpp")
        set(EMBED_H "${EMBED_OUTPUT_DIR}/unused.h")
    endif ()

    add_custom_command(
            OUTPUT "${COMMANDS_FILE}"
            COMMAND ${CMAKE_COMMAND} -E echo "Generating build commands..."
            COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_OUTPUT_DIR}"
            COMMAND ${PYTHON_EXECUTABLE} "${GENERATE_SCRIPT}"
                ${TARGET_NAME}
                ${SHADER_OUTPUT_DIR}
                ${OutputFormat}
                ${COMMANDS_FILE}
                "${SlangCompiler}"
                "${DxcLibraryDir}"
                ${SHADER_INPUT_DIR}
                "${SHADER_INCLUDE_LIST}"
                ${EMBED_ENABLED}
                ${EMBED_SCRIPT}
                ${EMBED_CPP}
                ${EMBED_H}
                $<BOOL:${KRYNE_ENGINE_OPTIMIZE_SPIRV}>
                ${ShaderListFiles}
            DEPENDS ${GENERATE_SCRIPT} ${ShaderListFiles}
            COMMENT "Parsing shader list"
    )

    message(STATUS "${CMAKE_MAKE_PROGRAM}")
    add_custom_target(${TARGET_NAME}_ShaderCommands
            COMMAND ${CMAKE_MAKE_PROGRAM}
            WORKING_DIRECTORY ${WORKING_DIR}
            DEPENDS ${COMMANDS_FILE}
            COMMENT "Shader Compilation [${TARGET_NAME}]"
    )

    get_target_property(folder ${TARGET_NAME} FOLDER)
    if (NOT folder STREQUAL "folder-NOTFOUND")
        set_target_properties(${TARGET_NAME}_ShaderCommands PROPERTIES FOLDER ${folder})
    endif ()

    set_target_properties(${TARGET_NAME} PROPERTIES SET_UP_COMPILE_COMMANDS ON)

    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_ShaderCommands)

    if (ARG_EMBED)
        set_source_files_properties(${EMBED_CPP} ${EMBED_H} PROPERTIES GENERATED ON)
        target_sources(${TARGET_NAME} PRIVATE ${EMBED_CPP})
        target_include_directories(${TARGET_NAME} PRIVATE ${EMBED_OUTPUT_DIR})
    endif ()
endfunction()

# target_declare_shader_library implementation
function(target_declare_shader_library TARGET_NAME LOCAL_SHADERS_DIR)
    set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${LOCAL_SHADERS_DIR}")
    set_target_properties(${TARGET_NAME} PROPERTIES SHADER_INCLUDE_DIR ${SHADER_DIR})
endfunction()

# target_link_shader_libraries implementation
function(target_link_shader_libraries TARGET_NAME)
    get_target_property(SET_UP ${TARGET_NAME} SET_UP_COMPILE_COMMANDS)
    if (SET_UP)
        message(FATAL_ERROR "Shader libraries must be linked before commands are set up")
    endif ()

    set(INCLUDE_LIST "")
    foreach (LINKED_TARGET_NAME IN LISTS ARGN)
        if (NOT TARGET ${LINKED_TARGET_NAME})
            message(FATAL_ERROR "Unknown target named '${LINKED_TARGET_NAME}'")
        endif ()
        get_target_property(SHADER_INCLUDE_DIR ${LINKED_TARGET_NAME} SHADER_INCLUDE_DIR)
        if (${SHADER_INCLUDE_DIR} STREQUAL "SHADER_INCLUDE_DIR-NOTFOUND")
            message(FATAL_ERROR "No shader lib was declared for '${LINKED_TARGET_NAME}'")
        endif ()
        list(APPEND INCLUDE_LIST ${SHADER_INCLUDE_DIR})
    endforeach ()
    set_target_properties(${TARGET_NAME} PROPERTIES SHADER_INCLUDE_LIST "${INCLUDE_LIST}")
endfunction()

function(target_shaders_dir_symlink TARGET_NAME)
    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Shaders")
    add_custom_command(
            TARGET ${TARGET_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${SHADER_OUTPUT_DIR} "${CMAKE_CURRENT_BINARY_DIR}/Shaders"
    )
endfunction()
