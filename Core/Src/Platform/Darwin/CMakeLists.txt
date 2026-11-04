set(KE_PLATFORM_IMPLEMENTATION_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/Alert.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cpu.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/FileSystem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Font.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Ipc.cpp
        PARENT_SCOPE)

# CoreFoundation - Alert.cpp (CFUserNotificationDisplayAlert)
find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation)
set(KE_PLATFORM_LIBS ${CORE_FOUNDATION_FRAMEWORK} PARENT_SCOPE)
