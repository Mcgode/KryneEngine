/**
* @file
* @author Max Godefroy
* @date 19/03/2022.
*/

#pragma once

#include "KryneEngine/Core/Graphics/GraphicsCommon.hpp"

namespace KryneEngine::Tests::Graphics
{
    inline GraphicsCommon::ApplicationInfo DefaultAppInfo()
    {
        GraphicsCommon::ApplicationInfo appInfo = {
            .m_applicationName = "Unit test app",
            .m_api =
#if defined(KE_GRAPHICS_API_DX12)
                GraphicsCommon::Api::DirectX12_1,
#elif defined(KE_GRAPHICS_API_VK)
                GraphicsCommon::Api::Vulkan_1_0,
#elif defined(KE_GRAPHICS_API_MTL)
                GraphicsCommon::Api::Metal_3,
#endif
            .m_features = {
                .m_present = false,
                .m_transferQueue = false,
            },
        };
        return appInfo;
    }
}
