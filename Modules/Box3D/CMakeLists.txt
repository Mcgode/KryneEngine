add_subdirectory(Extern/box3d-0.1.0)
set_target_properties(box3d PROPERTIES FOLDER "External")


add_library(KryneEngine_Modules_Box3D STATIC
        Src/Context.cpp
        Include/KryneEngine/Modules/Box3D/Context.hpp
)

set_target_properties(KryneEngine_Modules_Box3D PROPERTIES FOLDER "EngineModules")

target_include_directories(KryneEngine_Modules_Box3D PUBLIC Include)

target_link_libraries(KryneEngine_Modules_Box3D PUBLIC KryneEngine_Core box3d)