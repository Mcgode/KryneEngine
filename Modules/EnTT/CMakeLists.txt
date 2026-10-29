add_subdirectory(Extern/entt-4.0.0)

set_target_properties(EnTT PROPERTIES FOLDER "External")


add_library(KryneEngine_Modules_EnTT STATIC
        Src/Context.cpp
        Include/KryneEngine/Modules/EnTT/Context.hpp
)

set_target_properties(KryneEngine_Modules_EnTT PROPERTIES FOLDER "EngineModules")

target_include_directories(KryneEngine_Modules_EnTT PUBLIC Include)

target_link_libraries(KryneEngine_Modules_EnTT PUBLIC KryneEngine_Core EnTT::EnTT)

# Systems run on fibers across threads, type index generation must be atomic
target_compile_definitions(KryneEngine_Modules_EnTT PUBLIC ENTT_USE_ATOMIC)