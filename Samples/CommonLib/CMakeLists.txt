add_library(Samples_CommonLib STATIC
        Src/Scene/TorusKnot.cpp
        Include/Scene/TorusKnot.hpp
        Src/Scene/TorusKnotMeshGenerator.cpp
        Include/Scene/TorusKnotMeshGenerator.hpp
        Src/Scene/BoxMeshGenerator.cpp
        Include/Scene/BoxMeshGenerator.hpp
        Src/Scene/OrbitCamera.cpp
        Include/Scene/OrbitCamera.hpp
        Src/Scene/SunLight.cpp
        Include/Scene/SunLight.hpp
        Src/Rendering/DrawInstanceManager.cpp
        Include/Rendering/DrawInstanceManager.hpp
        Src/Rendering/MaterialManager.cpp
        Include/Rendering/MaterialManager.hpp
        Src/Rendering/PassDispatcher.cpp
        Include/Rendering/PassDispatcher.hpp
        Src/Rendering/Fullscreen/FullscreenPassCommon.cpp
        Include/Rendering/Fullscreen/FullscreenPassCommon.hpp
        Src/Rendering/Fullscreen/FullscreenPassConstants.cpp
        Include/Rendering/Fullscreen/FullscreenPassConstants.hpp
        Src/Rendering/Fullscreen/SkyPass.cpp
        Include/Rendering/Fullscreen/SkyPass.hpp
        Src/Rendering/Fullscreen/DeferredShadingPass.cpp
        Include/Rendering/Fullscreen/DeferredShadingPass.hpp
        Src/Rendering/Fullscreen/ColorMappingPass.cpp
        Include/Rendering/Fullscreen/ColorMappingPass.hpp
)

target_include_directories(Samples_CommonLib PUBLIC Include)
target_include_directories(Samples_CommonLib PRIVATE Src)

target_link_libraries(Samples_CommonLib PUBLIC
        KryneEngine_Modules_ImGui
        KryneEngine_Modules_RenderGraph)

# Declare CommonLib as a shader library so dependent samples can include its shared headers
# (e.g. FrameData.hlsl), then link the engine shader library and compile CommonLib's own shaders.
target_declare_shader_library(Samples_CommonLib Shaders/Samples/CommonLib)
target_link_shader_libraries(Samples_CommonLib ShaderLib)
target_compile_shaders(Samples_CommonLib Shaders Samples/CommonLib)
