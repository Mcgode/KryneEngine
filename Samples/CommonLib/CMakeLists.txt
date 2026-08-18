add_library(Samples_CommonLib STATIC
        Src/Scene/TorusKnot.cpp
        Include/Scene/TorusKnot.hpp
        Src/Scene/TorusKnotMeshGenerator.cpp
        Include/Scene/TorusKnotMeshGenerator.hpp
)

target_include_directories(Samples_CommonLib PUBLIC Include)
target_include_directories(Samples_CommonLib PRIVATE Src)

target_link_libraries(Samples_CommonLib PUBLIC KryneEngine_Modules_ImGui)