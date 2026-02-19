add_executable(UiDemo UiDemo.cpp
        TextureGenerator.cpp
        TextureGenerator.hpp
        UiCube.cpp
        UiCube.hpp)

target_link_libraries(UiDemo KryneEngine_Modules_GuiLib)
set_target_properties(UiDemo PROPERTIES FOLDER "EngineSamples")

target_compile_shaders(UiDemo Shaders Examples/UiDemo)

target_shaders_dir_symlink(UiDemo)
target_symlink_raw_resources(UiDemo KryneEngine_Modules_TextRendering)