
set(MSDFGEN_CORE_ONLY ON)
set(MSDFGEN_DISABLE_SVG ON)
set(MSDFGEN_BUILD_STANDALONE OFF)
set(MSDFGEN_USE_VCPKG OFF)
set(MSDFGEN_USE_SKIA OFF)
add_subdirectory(Extern/msdfgen_1.13)

add_library(KryneEngine_Modules_TextRendering STATIC
        Src/Font.cpp
        Include/KryneEngine/Modules/TextRendering/Font.hpp
        Src/FontManager.cpp
        Include/KryneEngine/Modules/TextRendering/FontManager.hpp
        Src/MsdfAtlasManager.cpp
        Include/KryneEngine/Modules/TextRendering/MsdfAtlasManager.hpp
        Src/SystemFont.cpp
        Include/KryneEngine/Modules/TextRendering/SystemFont.hpp
        Include/KryneEngine/Modules/TextRendering/FontCommon.hpp)

find_package(Freetype REQUIRED)

target_add_raw_resources_dir(KryneEngine_Modules_TextRendering BundledResources Modules/TextRendering)

target_include_directories(KryneEngine_Modules_TextRendering PUBLIC Include)
target_include_directories(KryneEngine_Modules_TextRendering PRIVATE Extern/msdfgen_1.13)

target_link_libraries(KryneEngine_Modules_TextRendering PRIVATE Freetype::Freetype msdfgen-core)
target_link_libraries(KryneEngine_Modules_TextRendering PUBLIC KryneEngine_Core KryneEngine_Modules_GraphicsUtils KryneEngine_Modules_Resources)
set_target_properties(KryneEngine_Modules_TextRendering PROPERTIES FOLDER "EngineModules")