set(KE_PLATFORM_IMPLEMENTATION_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/Alert.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cpu.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/FileSystem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Font.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Ipc.cpp
        PARENT_SCOPE)

# Win32 import libraries the MSVC toolchain would pull in via #pragma comment(lib),
# but which must be listed explicitly for the mingw / lld cross build.
#   dwrite  - Font.cpp (DWriteCreateFactory)
#   user32  - Alert.cpp (MessageBoxW)
set(KE_PLATFORM_LIBS
        dwrite
        user32
        PARENT_SCOPE)