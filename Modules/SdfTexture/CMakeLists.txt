add_library(KryneEngine_Modules_SdfTexture STATIC
        Include/KryneEngine/Modules/SdfTexture/Generator.hpp
        Src/Generator.cpp)

target_link_libraries(KryneEngine_Modules_SdfTexture KryneEngine_Core)
set_target_properties(KryneEngine_Modules_SdfTexture PROPERTIES FOLDER "EngineModules")

target_include_directories(KryneEngine_Modules_SdfTexture PUBLIC Include)
target_include_directories(KryneEngine_Modules_SdfTexture PRIVATE Src)