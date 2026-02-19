add_library(KryneEngine_Modules_Resources
        Src/RuntimeResourceSystem.cpp
        Include/KryneEngine/Modules/Resources/RuntimeResourceSystem.hpp
        Include/KryneEngine/Modules/Resources/ResourceTypeId.hpp
        Include/KryneEngine/Modules/Resources/ResourceEntry.hpp
        Include/KryneEngine/Modules/Resources/IResourceManager.hpp
        Include/KryneEngine/Modules/Resources/IResourceLoader.hpp
        Src/Loaders/SerialResourceLoader.cpp
        Include/KryneEngine/Modules/Resources/Loaders/SerialResourceLoader.hpp
        Include/KryneEngine/Modules/Resources/ResourceBase.hpp
)

target_include_directories(KryneEngine_Modules_Resources PUBLIC Include)

target_link_libraries(KryneEngine_Modules_Resources PUBLIC KryneEngine_Core)
set_target_properties(KryneEngine_Modules_Resources PROPERTIES FOLDER "EngineModules")