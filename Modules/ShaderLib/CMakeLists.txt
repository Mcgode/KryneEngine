add_library(ShaderLib INTERFACE)
set_target_properties(ShaderLib PROPERTIES FOLDER "EngineModules")

target_declare_shader_library(ShaderLib Shaders)
#target_compile_shaders(ShaderLib Shaders Modules/ShaderLib)