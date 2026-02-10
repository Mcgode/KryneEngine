set(RootDir ${CMAKE_SOURCE_DIR}/Core)

set(CommonSrc
        Include/KryneEngine/Core/Common/Assert.hpp
        Src/Common/Assert.cpp
        Include/KryneEngine/Core/Common/BitUtils.hpp
        Include/KryneEngine/Core/Common/EastlHelpers.hpp
        Include/KryneEngine/Core/Common/StringHelpers.hpp
        Include/KryneEngine/Core/Common/Types.hpp
        Src/Common/additional_impl.cpp
        Include/KryneEngine/Core/Common/Utils/Alignment.hpp
        Include/KryneEngine/Core/Common/Utils/MultiFrameTracking.hpp
        Src/Common/BitUtils.cpp
        Include/KryneEngine/Core/Common/Misc/EastlConfig.hpp
        Include/KryneEngine/Core/Common/Misc/Unroll.hpp
        Include/KryneEngine/Core/Common/Utils/Macros.hpp
        Src/Common/StringHelpers.cpp
)

set(FilesSrc
        Src/Files/FileWatcher.cpp
        Src/Files/FileWatcher.hpp
        Src/Files/WatchedDirectory.cpp
        Src/Files/WatchedDirectory.hpp
        Src/Files/FileSystemHelper.cpp
        Src/Files/FileSystemHelper.hpp
        Src/Files/IoQueryManager.cpp
        Src/Files/IoQueryManager.hpp
        Src/Files/File.cpp
        Src/Files/File.hpp
        )

set(GraphicsCommonFiles
        Src/Graphics/GraphicsContext.cpp
        Include/KryneEngine/Core/Graphics/GraphicsContext.hpp
        Include/KryneEngine/Core/Graphics/GraphicsCommon.hpp
        Include/KryneEngine/Core/Graphics/Texture.hpp
        Include/KryneEngine/Core/Graphics/Enums.hpp
        Src/Graphics/EnumHelpers.cpp
        Include/KryneEngine/Core/Graphics/EnumHelpers.hpp
        Include/KryneEngine/Core/Graphics/ResourceViews/RenderTargetView.hpp
        Include/KryneEngine/Core/Graphics/RenderPass.hpp
        Include/KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp
        Include/KryneEngine/Core/Graphics/MemoryBarriers.hpp
        Include/KryneEngine/Core/Graphics/Buffer.hpp
        Include/KryneEngine/Core/Graphics/Handles.hpp
        Include/KryneEngine/Core/Graphics/ShaderPipeline.hpp
        Include/KryneEngine/Core/Graphics/Drawing.hpp
        Include/KryneEngine/Core/Graphics/ResourceViews/BufferView.hpp
        Include/KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp
)

set(MathSrc
        Src/Math/Vector2.cpp
        Src/Math/Vector3.cpp
        Src/Math/Vector4.cpp
        Include/KryneEngine/Core/Math/Vector.hpp
        Include/KryneEngine/Core/Math/Vector2.hpp
        Include/KryneEngine/Core/Math/Vector3.hpp
        Include/KryneEngine/Core/Math/Vector4.hpp
        Include/KryneEngine/Core/Math/XSimdUtils.hpp
        Include/KryneEngine/Core/Math/Hashing.hpp
        Src/Math/Hashing.cpp
        Include/KryneEngine/Core/Math/Quaternion.hpp
        Include/KryneEngine/Core/Math/CoordinateSystem.hpp
        Include/KryneEngine/Core/Math/RotationConversion.hpp
        Src/Math/Matrix33.cpp
        Include/KryneEngine/Core/Math/Matrix33.hpp
        Include/KryneEngine/Core/Math/Matrix.hpp
        Src/Math/Matrix44.cpp
        Include/KryneEngine/Core/Math/Matrix44.hpp
        Include/KryneEngine/Core/Math/Transform.hpp
        Include/KryneEngine/Core/Math/Projection.hpp
        Include/KryneEngine/Core/Math/BoundingBox.hpp
        Include/KryneEngine/Core/Math/Float16.hpp
        Src/Math/Float16.cpp
        Include/KryneEngine/Core/Math/Color.hpp
)

set(MemorySrc
        Include/KryneEngine/Core/Memory/DynamicArray.hpp
        Include/KryneEngine/Core/Memory/RangeMapping.hpp
        Include/KryneEngine/Core/Memory/GenerationalPool.inl
        Include/KryneEngine/Core/Memory/GenerationalPool.hpp
        Include/KryneEngine/Core/Memory/SimplePool.inl
        Include/KryneEngine/Core/Memory/SimplePool.hpp
        Src/Memory/Allocators/TlsfAllocator.cpp
        Include/KryneEngine/Core/Memory/Allocators/TlsfAllocator.hpp
        Include/KryneEngine/Core/Memory/Heaps/TlsfHeap.hpp
        Src/Memory/Allocators/Allocator.cpp
        Include/KryneEngine/Core/Memory/Allocators/Allocator.hpp
        Include/KryneEngine/Core/Memory/Allocators/DefaultHeapHeapAllocationTracker.hpp
        Src/Memory/Allocators/DefaultHeapHeapAllocationTracker.cpp
        Include/KryneEngine/Core/Memory/UniquePtr.hpp
        Include/KryneEngine/Core/Memory/IndexAllocator.hpp
        Include/KryneEngine/Core/Memory/Containers/StableVector.hpp
        Include/KryneEngine/Core/Memory/Containers/StableVector.inl
        Include/KryneEngine/Core/Memory/Containers/VectorDeLinkedList.hpp
        Include/KryneEngine/Core/Memory/IntrusivePtr.hpp
        Src/Memory/Containers/ConcurrentQueue.cpp
        Include/KryneEngine/Core/Memory/Containers/ConcurrentQueue.hpp
        Include/KryneEngine/Core/Memory/Containers/Pool.hpp
        Include/KryneEngine/Core/Memory/Containers/FlatHashMap.hpp
        Include/KryneEngine/Core/Memory/Containers/FlatHashMap.inl
)

message(STATUS "Loading implementation for platform '${CMAKE_SYSTEM_NAME}'")
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Src/Platform/${CMAKE_SYSTEM_NAME}/CMakeLists.txt")
        message(FATAL_ERROR "Could not find platform implementation for platform ${CMAKE_SYSTEM_NAME}")
endif ()
add_subdirectory("Src/Platform/${CMAKE_SYSTEM_NAME}")

set(PlatformSrc
        Include/KryneEngine/Core/Platform/Windows.h
        Src/Platform/StdAlloc.cpp
        Include/KryneEngine/Core/Platform/StdAlloc.hpp
        Include/KryneEngine/Core/Platform/Platform.hpp
        ${KE_PLATFORM_IMPLEMENTATION_FILES}
)

set(ProfilingSrc
        Include/KryneEngine/Core/Profiling/TracyHeader.hpp
        Src/Profiling/TracyGpuProfilerContext.cpp
        Include/KryneEngine/Core/Profiling/TracyGpuProfilerContext.hpp
        Include/KryneEngine/Core/Profiling/TracyGpuScope.hpp
)

set(ThreadsSrc
        Src/Threads/FibersManager.cpp
        Include/KryneEngine/Core/Threads/FibersManager.hpp
        Src/Threads/FiberThread.cpp
        Include/KryneEngine/Core/Threads/FiberThread.hpp
        Src/Threads/HelperFunctions.cpp
        Include/KryneEngine/Core/Threads/HelperFunctions.hpp
        Include/KryneEngine/Core/Threads/FiberTls.inl
        Include/KryneEngine/Core/Threads/FiberTls.hpp
        Src/Threads/FiberJob.cpp
        Include/KryneEngine/Core/Threads/FiberJob.hpp
        Src/Threads/Internal/FiberContext.cpp
        Src/Threads/Internal/FiberContext.hpp
        Src/Threads/SyncCounterPool.cpp
        Include/KryneEngine/Core/Threads/SyncCounterPool.hpp
        Src/Threads/LightweightMutex.cpp
        Include/KryneEngine/Core/Threads/LightweightMutex.hpp
        Include/KryneEngine/Core/Threads/RwMutex.hpp
        Include/KryneEngine/Core/Threads/LightweightSemaphore.hpp
        Src/Threads/LightweightSemaphore.cpp
        Src/Threads/SpinLock.cpp
        Include/KryneEngine/Core/Threads/SpinLock.hpp
        Src/Threads/RwSpinLock.cpp
        Include/KryneEngine/Core/Threads/RwSpinLock.hpp
)

set(WindowSrc
        Src/Window/Window.cpp Include/KryneEngine/Core/Window/Window.hpp
        Src/Window/Input/InputManager.cpp Include/KryneEngine/Core/Window/Input/InputManager.hpp
        Include/KryneEngine/Core/Window/Input/KeyInputEvent.hpp
        Include/KryneEngine/Core/Window/GLFW/Input/KeyInputEvent.hpp
        Src/Window/GLFW/Input/KeyInputEvent.cpp
        Include/KryneEngine/Core/Window/Input/MouseInputEvent.hpp
        Include/KryneEngine/Core/Window/Input/Enums.hpp
)

if (GraphicsApi STREQUAL "VK")
        set(GraphicsFiles ${GraphicsCommonFiles}
                Src/Graphics/Vulkan/VkGraphicsContext.hpp
                Src/Graphics/Vulkan/VkGraphicsContext.cpp
                Src/Graphics/Vulkan/HelperFunctions.cpp
                Src/Graphics/Vulkan/HelperFunctions.hpp
                Src/Graphics/Vulkan/VkSwapChain.cpp
                Src/Graphics/Vulkan/VkSwapChain.hpp
                Src/Graphics/Vulkan/VkSurface.cpp
                Src/Graphics/Vulkan/VkSurface.hpp
                Src/Graphics/Vulkan/CommonStructures.hpp
                Src/Graphics/Vulkan/VkHeaders.hpp
                Src/Graphics/Vulkan/VkFrameContext.cpp
                Src/Graphics/Vulkan/VkFrameContext.hpp
                Src/Graphics/Vulkan/VkResources.cpp
                Src/Graphics/Vulkan/VkResources.hpp
                Src/Graphics/Vulkan/VkTypes.hpp
                Src/Graphics/Vulkan/VkDebugHandler.hpp
                Src/Graphics/Vulkan/VmaUsage.cpp
                Src/Graphics/Vulkan/VkDescriptorSetManager.cpp
                Src/Graphics/Vulkan/VkDescriptorSetManager.hpp
        )
elseif(GraphicsApi STREQUAL "DX12")
        set(GraphicsFiles ${GraphicsCommonFiles}
                Src/Graphics/DirectX12/Dx12GraphicsContext.cpp
                Src/Graphics/DirectX12/Dx12GraphicsContext.hpp
                Src/Graphics/DirectX12/Dx12Headers.hpp
                Src/Graphics/DirectX12/HelperFunction.cpp
                Src/Graphics/DirectX12/HelperFunctions.hpp
                Src/Graphics/DirectX12/Dx12SwapChain.cpp
                Src/Graphics/DirectX12/Dx12SwapChain.hpp
                Src/Graphics/DirectX12/Dx12FrameContext.cpp
                Src/Graphics/DirectX12/Dx12FrameContext.hpp
                Src/Graphics/DirectX12/Dx12Resources.cpp
                Src/Graphics/DirectX12/Dx12Resources.h
                Src/Graphics/DirectX12/Dx12Types.hpp
                Src/Graphics/DirectX12/Dx12DescriptorSetManager.cpp
                Src/Graphics/DirectX12/Dx12DescriptorSetManager.hpp
        )
elseif (GraphicsApi STREQUAL "MTL")
        set(GraphicsFiles ${GraphicsCommonFiles}
                Src/Graphics/Metal/Implementation/metal-cpp.cpp
                Src/Graphics/Metal/MetalHeaders.hpp
                Src/Graphics/Metal/MetalGraphicsContext.hpp
                Src/Graphics/Metal/MetalGraphicsContext.cpp
                Src/Graphics/Metal/MetalGraphicsContextInit.cpp
                Src/Graphics/Metal/Helpers/NsPtr.hpp
                Src/Graphics/Metal/MetalFrameContext.cpp
                Src/Graphics/Metal/MetalFrameContext.hpp
                Src/Graphics/Metal/MetalTypes.hpp
                Src/Graphics/Metal/MetalSwapChain.mm
                Src/Graphics/Metal/MetalSwapChain.hpp
                Src/Graphics/Metal/MetalResources.cpp
                Src/Graphics/Metal/MetalResources.hpp
                Src/Graphics/Metal/Helpers/EnumConverters.cpp
                Src/Graphics/Metal/Helpers/EnumConverters.hpp
                Src/Graphics/Metal/MetalArgumentBufferManager.cpp
                Src/Graphics/Metal/MetalArgumentBufferManager.hpp
                Src/Graphics/Metal/Helpers/RenderState.hpp
        )
endif()

add_library(KryneEngine_Core
        ${CommonSrc}
        ${FilesSrc}
        ${GraphicsFiles}
        ${MathSrc}
        ${MemorySrc}
        ${PlatformSrc}
        ${ProfilingSrc}
        ${ThreadsSrc}
        ${WindowSrc})
target_link_libraries(KryneEngine_Core EASTL glfw glm Tracy::TracyClient moodycamel Boost::context xsimd)

if (GraphicsApi STREQUAL "VK")
        message(STATUS "Finding and linking Vulkan lib")
        find_package(Vulkan REQUIRED)

        message(STATUS "Vulkan include dirs: " ${Vulkan_INCLUDE_DIRS})
        target_include_directories(KryneEngine_Core PRIVATE ${Vulkan_INCLUDE_DIRS})

        message(STATUS "Vulkan lib: " ${Vulkan_LIBRARIES})
        target_link_libraries(KryneEngine_Core ${Vulkan_LIBRARIES} VulkanMemoryAllocator)
elseif(GraphicsApi STREQUAL "DX12")
        message(STATUS "Linking DirectX12 libs")
        target_link_libraries(KryneEngine_Core d3d12 dxgi d3dcompiler dxguid DirectX-Headers D3D12MemoryAllocator)
elseif(GraphicsApi STREQUAL "MTL")
        find_library(METAL_FRAMEWORK Metal)
        find_library(METALKIT_FRAMEWORK MetalKit)
        find_library(QUARTZCORE_FRAMEWORK QuartzCore)
        find_library(COCOA_FRAMEWORK Cocoa)

        message(STATUS "Linking Metal libs")
        target_link_libraries(KryneEngine_Core metal-cpp metal-cpp-extensions ${METAL_FRAMEWORK} ${METALKIT_FRAMEWORK} ${QUARTZCORE_FRAMEWORK} ${COCOA_FRAMEWORK})
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(KryneEngine_Core PRIVATE -Werror=switch)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(KryneEngine_Core PRIVATE /we4062)
endif()

target_compile_definitions(KryneEngine_Core PUBLIC EASTL_USER_CONFIG_HEADER="KryneEngine/Core/Common/Misc/EastlConfig.hpp")

target_include_directories(KryneEngine_Core PUBLIC Include)
target_include_directories(KryneEngine_Core PRIVATE Src)

AddCoverage(KryneEngine_Core)