add_library(KryneEngine_Modules_GuiLib STATIC
        Src/ClayImpl.cpp
        Src/Context.cpp
        Include/KryneEngine/Modules/GuiLib/Context.hpp
        Include/KryneEngine/Modules/GuiLib/IGuiRenderer.hpp
        Include/KryneEngine/Modules/GuiLib/TextureRegion.hpp
        Src/GuiRenderers/BasicGuiRenderer.cpp
        Include/KryneEngine/Modules/GuiLib/GuiRenderers/BasicGuiRenderer.hpp
)

target_include_directories(KryneEngine_Modules_GuiLib PUBLIC Include Extern/Clay)

target_link_libraries(KryneEngine_Modules_GuiLib PUBLIC KryneEngine_Core KryneEngine_Modules_GraphicsUtils KryneEngine_Modules_TextRendering)
set_target_properties(KryneEngine_Modules_GuiLib PROPERTIES FOLDER "EngineModules")

target_link_shader_libraries(KryneEngine_Modules_GuiLib ShaderLib)
target_compile_shaders(KryneEngine_Modules_GuiLib Shaders Modules/GuiLib)