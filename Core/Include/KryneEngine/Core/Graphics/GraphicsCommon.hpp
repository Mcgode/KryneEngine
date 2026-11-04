/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2022.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"

namespace KryneEngine::GraphicsCommon
{
    enum class Api
    {
        None,

        Vulkan_1_0,
        Vulkan_1_1,
        Vulkan_1_2,
        Vulkan_1_3,

        Vulkan_Start = Vulkan_1_0,
        Vulkan_End = Vulkan_1_3,

        DirectX12_0,
        DirectX12_1,
        DirectX12_2,

        DirectX12_Start = DirectX12_0,
        DirectX12_End = DirectX12_2,

        Metal_4,
    };

    enum class SoftEnable: u8
    {
        Disabled,
        TryEnable,
        ForceEnabled
    };

    /**
     * @brief Number of frames the engine keeps in flight (and hence the swap chain image count).
     *
     * @details
     * A strict, explicit choice — it drives `GraphicsContext::GetFrameContextCount()` and is *not*
     * negotiated against window/surface capabilities. Creating a swap chain that cannot honour the
     * requested count is a hard error. The underlying value is the count itself.
     */
    enum class BufferingMode : u8
    {
        Single = 1,
        Double = 2,
        Triple = 3,
    };

    /**
     * @brief Window / presentation-surface preferences.
     *
     * @details
     * Independent of @ref ApplicationInfo — passed to window creation (size, decorations).
     */
    struct DisplayOptions
    {
        u16 m_width = 1280;
        u16 m_height = 720;

        bool m_fullscreen = false;
        bool m_resizableWindow = false;
        /// @brief Whether the OS draws a title bar / border. `false` for borderless windows
        ///        (e.g. Dear ImGui secondary viewports).
        bool m_decorated = true;
    };

    struct ApplicationInfo
    {
        eastl::string m_applicationName = "Unnamed app";
        Version m_applicationVersion {};

        Version m_engineVersion { 1, 0, 0 };
        Api m_api = Api::None;

        BufferingMode m_bufferingMode = BufferingMode::Double;

        // Member order is chosen so the struct has no padding bytes: every `SoftEnable`/`bool`
        // (all 1-byte) comes first and the 4-byte `m_gpuTimestampBufferCapacity` lands last on a
        // natural boundary. This keeps the struct byte-comparable (see the unit tests).
        struct Features
        {
            SoftEnable m_validationLayers = SoftEnable::TryEnable;
            SoftEnable m_debugTags = SoftEnable::TryEnable;
            SoftEnable m_gpuTimestamps = SoftEnable::TryEnable;

            /// @brief Enables geometry shaders. Also required to read `SV_PrimitiveID` in a fragment
            ///        shader without a geometry/tessellation stage (a SPIR-V capability requirement).
            ///        Not universally supported (e.g. Metal / MoltenVK have no geometry shaders), hence
            ///        a @ref SoftEnable: `ForceEnabled` fails context creation if unavailable, `TryEnable` silently skips.
            SoftEnable m_geometryShader = SoftEnable::Disabled;

            /**
             * @brief Enables indirect drawing. Vulkan require explicit feature opt-in, as it isn't universally
             * supported there.
             */
            SoftEnable m_drawIndirect = SoftEnable::TryEnable;

            /// @brief Allows arrayed descriptor bindings to be only partially written — unwritten slots
            ///        that the shader never dynamically accesses stay legal (Vulkan
            ///        `VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT`). Not guaranteed on Vulkan 1.0 / older
            ///        Android; query @ref GraphicsContext::SupportsPartiallyBoundDescriptors after creation
            ///        and, when unavailable, fully populate every arrayed binding instead.
            ///        `ForceEnabled` fails context creation if unavailable, `TryEnable` silently falls back.
            SoftEnable m_partiallyBoundDescriptors = SoftEnable::Disabled;

            bool m_graphics = true;
            bool m_present = true;
            bool m_transfer = true;
            bool m_compute = true;

            bool m_transferQueue = true;
            bool m_asyncCompute = false;
            bool m_concurrentQueues = true;

            u32 m_gpuTimestampBufferCapacity = 4'096;
        }
        m_features {};

        [[nodiscard]] bool IsVulkanApi() const
        {
            return m_api >= Api::Vulkan_Start && m_api <= Api::Vulkan_End;
        }

        [[nodiscard]] bool IsDirectX12Api() const
        {
            return m_api >= Api::DirectX12_Start && m_api <= Api::DirectX12_End;
        }

        [[nodiscard]] bool IsMetalApi() const
        {
            return m_api == Api::Metal_4;
        }
    };

    /**
     * @brief Reason why `GraphicsContext::Create` failed.
     *
     * @details
     * Only failures that depend on the host machine are reported this way, so that an application can
     * enforce a minimum set of features and tell the user explicitly why it cannot run. An invalid
     * @ref ApplicationInfo (an unsupported combination of queue features, for example) is a programming
     * error, and still asserts.
     */
    enum class ContextCreationError : u8
    {
        /// @brief The context was created successfully.
        None,
        /// @brief The requested graphics API or API version is not available on this host.
        ApiNotSupported,
        /// @brief No GPU supports the requested API version, queues and presentation.
        NoCompatibleDevice,
        /// @brief A compatible GPU was found, but its device or core objects could not be created.
        DeviceCreationFailed,
        /// @brief `m_features.m_present` is set, but the host cannot present to a window.
        PresentNotSupported,
        /// @brief `m_validationLayers` is `ForceEnabled`, but validation layers are not available.
        ValidationLayersNotSupported,
        /// @brief `m_debugTags` is `ForceEnabled`, but debug tags are not available.
        DebugTagsNotSupported,
        /// @brief `m_gpuTimestamps` is `ForceEnabled`, but GPU timestamps are not available.
        GpuTimestampsNotSupported,
        /// @brief `m_geometryShader` is `ForceEnabled`, but the GPU does not support geometry shaders.
        GeometryShaderNotSupported,
        /// @brief `m_drawIndirect` is `ForceEnabled`, but the GPU does not support it.
        DrawIndirectNotSupported,
        /// @brief `m_partiallyBoundDescriptors` is `ForceEnabled`, but the GPU does not support them.
        PartiallyBoundDescriptorsNotSupported,
    };

    /// @brief Returns a human-readable description of a @ref ContextCreationError.
    [[nodiscard]] constexpr const char* ToString(const ContextCreationError _error)
    {
        switch (_error)
        {
        case ContextCreationError::None:
            return "No error";
        case ContextCreationError::ApiNotSupported:
            return "The requested graphics API version is not supported on this machine";
        case ContextCreationError::NoCompatibleDevice:
            return "No compatible GPU was found";
        case ContextCreationError::DeviceCreationFailed:
            return "The GPU device could not be initialized";
        case ContextCreationError::PresentNotSupported:
            return "Presenting to a window is not supported on this machine";
        case ContextCreationError::ValidationLayersNotSupported:
            return "Graphics validation layers are not available on this machine";
        case ContextCreationError::DebugTagsNotSupported:
            return "Graphics debug tags are not supported on this machine";
        case ContextCreationError::GpuTimestampsNotSupported:
            return "The GPU does not support timestamp queries";
        case ContextCreationError::GeometryShaderNotSupported:
            return "The GPU does not support geometry shaders";
        case ContextCreationError::DrawIndirectNotSupported:
            return "The GPU does not support indirect drawing";
        case ContextCreationError::PartiallyBoundDescriptorsNotSupported:
            return "The GPU does not support partially bound descriptors";
        }
        return "Unknown error";
    }
}