set(KE_PLATFORM_IMPLEMENTATION_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/Cpu.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/FileSystem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Font.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Ipc.cpp
        PARENT_SCOPE)

find_library(FONT_CONFIG_LIB NAMES fontconfig)

if (NOT FONT_CONFIG_LIB)
    message(FATAL_ERROR "Unable to find FontConfig library")
endif ()

set(KE_PLATFORM_LIBS ${FONT_CONFIG_LIB} PARENT_SCOPE)