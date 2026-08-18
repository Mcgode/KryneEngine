
# Set up Render graph demo executable
add_executable(RenderGraphDemo
        RenderGraphDemo.cpp
        Scene/TorusKnotMeshGenerator.cpp
        Scene/TorusKnotMeshGenerator.hpp
        Scene/SceneManager.cpp
        Scene/SceneManager.hpp
        Scene/TorusKnot.cpp
        Scene/TorusKnot.hpp
        Scene/OrbitCamera.cpp
        Scene/OrbitCamera.hpp
        Scene/SunLight.cpp
        Scene/SunLight.hpp
        Rendering/DeferredShadingPass.cpp
        Rendering/DeferredShadingPass.hpp
        Rendering/FullscreenPassCommon.cpp
        Rendering/FullscreenPassCommon.hpp
        Rendering/SkyPass.cpp
        Rendering/SkyPass.hpp
        Rendering/ColorMappingPass.cpp
        Rendering/ColorMappingPass.hpp
        Rendering/DeferredShadowPass.cpp
        Rendering/DeferredShadowPass.hpp
        Rendering/GiPass.cpp
        Rendering/GiPass.hpp
)

# Link to cpp libraries
target_link_libraries(RenderGraphDemo
        # KryneEngine_Modules_RenderGraph   (bundled with RenderGraphDebug)
        # KryneEngine_Modules_ImGui         (bundled with RenderGraphDebug)
        KryneEngine_Modules_RenderGraphDebug
)
set_target_properties(RenderGraphDemo PROPERTIES FOLDER "EngineSamples")

# Link shader libraries and compile sample shaders
target_link_shader_libraries(RenderGraphDemo ShaderLib)
target_compile_shaders(RenderGraphDemo Shaders Examples/RenderGraphDemo)

# Symlink compiled shaders directory
target_shaders_dir_symlink(RenderGraphDemo)

CopyDLLs(RenderGraphDemo)