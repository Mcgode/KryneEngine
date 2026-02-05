
# Fetch DirectX shader compiler executable
if (LINUX)
    set(DirectXShaderCompiler "${CMAKE_CURRENT_SOURCE_DIR}/External/DirectXCompiler/linux/bin/dxc")
elseif (WIN32)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
        set(DirectXShaderCompiler "${CMAKE_CURRENT_SOURCE_DIR}/External/DirectXCompiler/win32/bin/x64/dxc.exe")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86")
        set(DirectXShaderCompiler "${CMAKE_CURRENT_SOURCE_DIR}/External/DirectXCompiler/win32/bin/x86/dxc.exe")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64")
        set(DirectXShaderCompiler "${CMAKE_CURRENT_SOURCE_DIR}/External/DirectXCompiler/win32/bin/arm64/dxc.exe")
    else ()
        message(FATAL_ERROR "Unsupported processor ${CMAKE_SYSTEM_PROCESSOR}")
    endif ()
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(DirectXShaderCompiler "${CMAKE_CURRENT_SOURCE_DIR}/External/DirectXCompiler/macos/bin/dxc")
else ()
    message(FATAL_ERROR "Platform unsupported")
endif ()

# Check that we found the executable
if (EXISTS "${DirectXShaderCompiler}")
    message(STATUS "Will compile shaders using '${DirectXShaderCompiler}'")
else ()
    message(FATAL_ERROR "Unable to find '${DirectXShaderCompiler}'")
endif ()

# Add dxc to passed shader tools
set(ShaderTools "dxc=${DirectXShaderCompiler}")

#
if (GraphicsApi STREQUAL "MTL")
    set(NeedSpirVCross ON)
else ()
    set(NeedSpirVCross OFF)
endif ()

# Optionally fetch spirv-cross executable
if (NeedSpirVCross)
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        set(SpirVCross "${CMAKE_CURRENT_SOURCE_DIR}/External/spirv-cross/macos/bin/spirv-cross")
    else ()
        message(FATAL_ERROR "SpirV-Cross: Platform unsupported")
    endif ()

    if (EXISTS "${SpirVCross}")
        message(STATUS "Will cross-compile shaders using '${SpirVCross}'")
    else ()
        message(FATAL_ERROR "Unable to find '${SpirVCross}'")
    endif ()

    set(ShaderTools "${ShaderTools}%%spirv-cross=${SpirVCross}")
endif ()

# Global variables set
set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Shaders")
set(SHADER_BUILD_OUTPUT_DIR "${CMAKE_BINARY_DIR}/ShaderBuild")
set(GENERATE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/CMake/ShaderListParser.py")
find_package(Python3 REQUIRED)

# target_compile_shaders implementation
function(target_compile_shaders TARGET_NAME LOCAL_SHADERS_DIR OUTPUT_DIR_NAME)
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

    set(COMMANDS_FILE "${BUILD_OUTPUT_DIR}/build.ninja")
    set(WORKING_DIR ${BUILD_OUTPUT_DIR})

    add_custom_command(
            OUTPUT "${COMMANDS_FILE}"
            COMMAND ${CMAKE_COMMAND} -E echo "Generating build commands..."
            COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_OUTPUT_DIR}"
            COMMAND ${Python3_EXECUTABLE} "${GENERATE_SCRIPT}"
                ${TARGET_NAME}
                ${SHADER_OUTPUT_DIR}
                ${OutputFormat}
                ${COMMANDS_FILE}
                "${ShaderTools}"
                ${SHADER_INPUT_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}
                "${SHADER_INCLUDE_LIST}"
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

    set_target_properties(${TARGET_NAME} PROPERTIES SET_UP_COMPILE_COMMANDS ON)

    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_ShaderCommands)
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
    add_custom_command(
            TARGET ${TARGET_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${SHADER_OUTPUT_DIR} "${CMAKE_CURRENT_BINARY_DIR}/Shaders"
    )
endfunction()
