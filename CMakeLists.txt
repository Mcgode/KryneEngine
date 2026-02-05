cmake_minimum_required(VERSION 3.20)
project(KryneEngine2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=thread -fno-omit-frame-pointer")
#set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address -fsanitize=thread")

include(./CMake/BuildSystem.cmake)
include(./CMake/ShaderCompilation.cmake)
include(./CMake/ResourceUtils.cmake)

if (WIN32)
    add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)
endif ()

option(KRYNE_ENGINE_BUILD_MODULES "Build modules for KryneEngine" ON)
option(KRYNE_ENGINE_BUILD_TOOLS "Build tools for KryneEngine" ON)
option(KRYNE_ENGINE_BUILD_SAMPLES "Build samples for KryneEngine" ON)
option(KRYNE_ENGINE_ENABLE_TESTING "Toggle testing for KryneEngine" ON)

option(KRYNE_ENGINE_PROFILE_MEMORY_ALLOCATIONS "Toggles Tracy memory allocation tracking and profiling" ON)
option(KRYNE_ENGINE_TRACK_DEFAULT_HEAP_ALLOCATIONS "Toggles default heap allocation tracking" ON)

add_subdirectory(External)
add_subdirectory(Core)

target_compile_definitions(KryneEngine_Core PUBLIC "-DKE_GRAPHICS_API_${GraphicsApi}=1")
if (TypeName STREQUAL "Release")
    target_compile_definitions(KryneEngine_Core PUBLIC "-DKE_FINAL=1")
endif()

if (KRYNE_ENGINE_TRACK_DEFAULT_HEAP_ALLOCATIONS)
    message(STATUS "Default heap allocation tracking is enabled")
    target_compile_definitions(KryneEngine_Core PUBLIC KE_TRACK_DEFAULT_HEAP_ALLOCATIONS=1)
endif ()

if (KRYNE_ENGINE_PROFILE_MEMORY_ALLOCATIONS)
    message(STATUS "Memory allocation profiling is enabled")
    target_compile_definitions(KryneEngine_Core PUBLIC KE_PROFILE_MEMORY_ALLOCATIONS=1)
endif ()

if (KRYNE_ENGINE_BUILD_MODULES)
    message(STATUS "Will build modules for KryneEngine")
    add_subdirectory(Modules)
endif ()

if (KRYNE_ENGINE_BUILD_TOOLS)
    message(STATUS "Will build tools for KryneEngine")
    add_subdirectory(Tools)
endif ()

if (KRYNE_ENGINE_BUILD_SAMPLES)
    message(STATUS "Will build samples for KryneEngine")
    add_subdirectory(Examples)
endif ()

if (KRYNE_ENGINE_ENABLE_TESTING)
    message(STATUS "KryneEngine tests are enabled")
    enable_testing()
    add_subdirectory(Tests)
endif ()