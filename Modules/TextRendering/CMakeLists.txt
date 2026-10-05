
set(MSDFGEN_CORE_ONLY ON)
set(MSDFGEN_DISABLE_SVG ON)
set(MSDFGEN_BUILD_STANDALONE OFF)
set(MSDFGEN_USE_VCPKG OFF)
set(MSDFGEN_USE_SKIA OFF)
# Match the dynamic CRT (/MD) used by the rest of the engine, otherwise MSVC fails with LNK2038
set(MSDFGEN_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
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
        Include/KryneEngine/Modules/TextRendering/FontCommon.hpp
        Src/FontFiles/PreBakedFontFile.cpp
        Include/KryneEngine/Modules/TextRendering/FontFiles/PreBakedFontFile.hpp
        Src/FontFiles/FreetypeFontFile.cpp
        Include/KryneEngine/Modules/TextRendering/FontFiles/FreetypeFontFile.hpp
        Include/KryneEngine/Modules/TextRendering/Utils/FreetypeFunctionHelpers.hpp
        Include/KryneEngine/Modules/TextRendering/Utils/MsdfGenFunctionHelpers.hpp)

# Prefer the package config: it propagates freetype's own dependencies (png, zlib, bz2, brotli),
# which a static build (e.g. vcpkg x64-windows-static-md) needs at link time.
find_package(Freetype CONFIG QUIET)
if (NOT Freetype_FOUND)
    find_package(Freetype REQUIRED)
endif ()

target_add_raw_resources_dir(KryneEngine_Modules_TextRendering BundledResources Modules/TextRendering)

target_include_directories(KryneEngine_Modules_TextRendering PUBLIC Include)
target_include_directories(KryneEngine_Modules_TextRendering PRIVATE Extern/msdfgen_1.13)

target_link_libraries(KryneEngine_Modules_TextRendering PRIVATE Freetype::Freetype msdfgen-core libzstd)
target_link_libraries(KryneEngine_Modules_TextRendering PUBLIC KryneEngine_Core_Link KryneEngine_Modules_GraphicsUtils KryneEngine_Modules_Resources)
set_target_properties(KryneEngine_Modules_TextRendering PROPERTIES FOLDER "EngineModules")