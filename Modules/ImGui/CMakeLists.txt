add_library(KryneEngine_Modules_ImGui STATIC
        Include/KryneEngine/Modules/ImGui/Context.hpp
        Src/Context.cpp
        Src/Input.cpp
        Src/Input.hpp
)
set_target_properties(KryneEngine_Modules_ImGui PROPERTIES FOLDER "EngineModules")

target_link_shader_libraries(KryneEngine_Modules_ImGui ShaderLib)
target_compile_shaders(KryneEngine_Modules_ImGui Shaders Modules/ImGui)

target_link_libraries(KryneEngine_Modules_ImGui
        KryneEngine_Core
        DearImgui
        KryneEngine_Modules_GraphicsUtils)

target_include_directories(KryneEngine_Modules_ImGui PUBLIC Include)
target_include_directories(KryneEngine_Modules_ImGui PRIVATE Src)
