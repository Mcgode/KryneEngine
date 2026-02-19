add_library(KryneEngine_Modules_GraphicsUtils
        Include/KryneEngine/Modules/GraphicsUtils/DynamicBuffer.hpp
        Src/DynamicBuffer.cpp
        Src/Allocators/AtlasShelfAllocator.cpp
        Include/KryneEngine/Modules/GraphicsUtils/Allocators/AtlasShelfAllocator.hpp
)

target_link_libraries(KryneEngine_Modules_GraphicsUtils KryneEngine_Core)
set_target_properties(KryneEngine_Modules_GraphicsUtils PROPERTIES FOLDER "EngineModules")

target_include_directories(KryneEngine_Modules_GraphicsUtils PUBLIC Include)
target_include_directories(KryneEngine_Modules_GraphicsUtils PRIVATE Src)
