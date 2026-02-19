add_library(KryneEngine_Modules_ShaderReflection STATIC
        Src/Blob.cpp
        Include/KryneEngine/Modules/ShaderReflection/Blob.hpp
        Include/KryneEngine/Modules/ShaderReflection/Input/EntryPoint.hpp
        Include/KryneEngine/Modules/ShaderReflection/Input/PushConstantInput.hpp
        Include/KryneEngine/Modules/ShaderReflection/Input/DescriptorSetInput.hpp
        Include/KryneEngine/Modules/ShaderReflection/Input/DescriptorInput.hpp
)

target_include_directories(KryneEngine_Modules_ShaderReflection PUBLIC Include)

target_link_libraries(KryneEngine_Modules_ShaderReflection PUBLIC KryneEngine_Core)
set_target_properties(KryneEngine_Modules_ShaderReflection PROPERTIES FOLDER "EngineModules")