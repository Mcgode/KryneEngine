add_subdirectory(ShaderLib)
add_subdirectory(FileSystem)
add_subdirectory(Resources)
add_subdirectory(GraphicsUtils)
add_subdirectory(GuiLib)
add_subdirectory(ImGui)
add_subdirectory(RenderGraph)
add_subdirectory(RenderGraphDebug)
add_subdirectory(ShaderReflection)
add_subdirectory(SdfTexture)
add_subdirectory(TextRendering)

add_library(KryneEngine_Modules INTERFACE)
target_link_libraries(KryneEngine_Modules INTERFACE
        KryneEngine_Modules_Resources
        KryneEngine_Modules_GraphicsUtils
        KryneEngine_Modules_ImGui
        KryneEngine_Modules_RenderGraph
        KryneEngine_Modules_RenderGraphDebug
        KryneEngine_Modules_ShaderReflection
        KryneEngine_Modules_SdfTexture
        KryneEngine_Modules_TextRendering
)

add_library(KryneEngine INTERFACE
        TextRendering/Src/Font.cpp
        TextRendering/Include/KryneEngine/Modules/TextRendering/Font.hpp)
target_link_libraries(KryneEngine INTERFACE KryneEngine_Core KryneEngine_Modules)