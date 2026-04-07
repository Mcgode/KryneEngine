
add_library(KryneEngine_Modules_FileSystem STATIC
        Src/VirtualFileSystem.cpp
        Include/KryneEngine/Modules/FileSystem/VirtualFileSystem.hpp
        Src/ReadOnlyFile.cpp
        Include/KryneEngine/Modules/FileSystem/ReadOnlyFile.hpp
        Src/ReadOnlyFile.cpp
        Include/KryneEngine/Modules/FileSystem/ReadOnlyFile.hpp
        Include/KryneEngine/Modules/FileSystem/Flags.hpp
)

target_include_directories(KryneEngine_Modules_FileSystem PUBLIC Include)
target_include_directories(KryneEngine_Modules_FileSystem PRIVATE Src)
target_link_libraries(KryneEngine_Modules_FileSystem PUBLIC KryneEngine_Core)
