add_executable(PhysicsDemo
        PhysicsDemo.cpp
        Src/SceneManager.cpp
        Src/SceneManager.hpp
        Src/PassTypes.hpp
        Src/RenderTargetFormats.hpp
)

target_link_libraries(PhysicsDemo
        Samples_CommonLib
        KryneEngine_Modules_RenderGraph
        KryneEngine_Modules_Box3D
)

set_target_properties(PhysicsDemo PROPERTIES FOLDER "EngineSamples")

# Link shader libraries and compile sample shaders
target_link_shader_libraries(PhysicsDemo ShaderLib)
target_compile_shaders(PhysicsDemo Shaders Samples/PhysicsDemo)

# Symlink compiled shaders directory
target_shaders_dir_symlink(PhysicsDemo)

CopyDLLs(PhysicsDemo)