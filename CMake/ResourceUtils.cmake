
option(KE_RAW_RESOURCE_HARD_COPY "Do a file copy of resources" OFF)

set(COMMON_RAW_RESOURCES_DIR "${CMAKE_BINARY_DIR}/Resources")
message(STATUS "Common resources dir: ${COMMON_RAW_RESOURCES_DIR}")

if (NOT EXISTS "${COMMON_RAW_RESOURCES_DIR}")
    file(MAKE_DIRECTORY "${COMMON_RAW_RESOURCES_DIR}")
endif ()

function(target_add_raw_resources_dir TARGET_NAME SRC_DIR TARGET_DIR_NAME)
    set(INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}")

    if (NOT EXISTS "${INPUT_DIR}")
        message(FATAL_ERROR "Cannot find directory '${INPUT_DIR}'")
    endif ()

    set(COMMON_RAW_RESOURCES_DIR "${CMAKE_BINARY_DIR}/Resources")
    set(OUTPUT_DIR "${COMMON_RAW_RESOURCES_DIR}/${TARGET_DIR_NAME}")

    if (KE_RAW_RESOURCE_HARD_COPY)
        add_custom_command(
                TARGET ${TARGET_NAME}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E remove_directory "${OUTPUT_DIR}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${INPUT_DIR}" "${OUTPUT_DIR}"
        )
    else ()
        get_filename_component(PARENT_DIR "${OUTPUT_DIR}" DIRECTORY)
        if (NOT EXISTS "${PARENT_DIR}")
            file(MAKE_DIRECTORY "${PARENT_DIR}")
        endif ()
        message(STATUS "Parent dir: ${PARENT_DIR}")

        add_custom_command(
                TARGET ${TARGET_NAME}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E remove_directory "${OUTPUT_DIR}"
                COMMAND ${CMAKE_COMMAND} -E create_symlink "${INPUT_DIR}" "${OUTPUT_DIR}"
        )
    endif ()

    set_target_properties(${TARGET_NAME} PROPERTIES
            RAW_RESOURCES_DIR "${TARGET_DIR_NAME}"
    )
endfunction()

function(target_symlink_raw_resources TARGET_NAME)
    foreach (LINKED_TARGET_NAME IN LISTS ARGN)
        if (NOT TARGET ${LINKED_TARGET_NAME})
            message(FATAL_ERROR "Unknown target: ${LINKED_TARGET_NAME}")
        endif ()

        get_target_property(RAW_RESOURCES_DIR ${LINKED_TARGET_NAME} RAW_RESOURCES_DIR)
        if (${RAW_RESOURCES_DIR} STREQUAL "RAW_RESOURCES_DIR-NOTFOUND")
            message(FATAL_ERROR "No raw resource dir was declared for '${LINKED_TARGET_NAME}'")
        endif ()

        set(TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/Resources/${RAW_RESOURCES_DIR}")
        get_filename_component(PARENT_DIR "${TARGET_DIR}" DIRECTORY)
        if (NOT EXISTS "${PARENT_DIR}")
            file(MAKE_DIRECTORY "${PARENT_DIR}")
        endif ()

        add_custom_command(
                TARGET ${TARGET_NAME}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E create_symlink "${COMMON_RAW_RESOURCES_DIR}/${RAW_RESOURCES_DIR}" "${TARGET_DIR}"
                COMMENT "Symlinked Resources/${RAW_RESOURCES_DIR}"
        )
    endforeach ()
endfunction()