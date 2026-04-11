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

        Metal_3,
    };

    enum class SoftEnable: u8
    {
        Disabled,
        TryEnable,
        ForceEnabled
    };

    struct ApplicationInfo
    {
        eastl::string m_applicationName = "Unnamed app";
        Version m_applicationVersion {};

        Version m_engineVersion { 1, 0, 0 };
        Api m_api = Api::None;

        struct Features
        {
            SoftEnable m_validationLayers = SoftEnable::TryEnable;
            SoftEnable m_debugTags = SoftEnable::TryEnable;
            SoftEnable m_gpuTimestamps = SoftEnable::TryEnable;
            u32 m_gpuTimestampBufferCapacity = 4'096;

            bool m_graphics = true;
            bool m_present = true;
            bool m_transfer = true;
            bool m_compute = true;

            bool m_transferQueue = true;
            bool m_asyncCompute = false;
            bool m_concurrentQueues = true;
        }
        m_features {};

        struct DisplayOptions
        {
            u16 m_width = 1280;
            u16 m_height = 720;

            SoftEnable m_sRgbPresent = SoftEnable::TryEnable;
            SoftEnable m_tripleBuffering = SoftEnable::TryEnable;

            bool m_fullscreen = false;
            bool m_resizableWindow = false;
        }
        m_displayOptions {};

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
            return m_api == Api::Metal_3;
        }
    };
}