/**
 * @file
 * @author Max Godefroy
 * @date 05/07/2024.
 */

#pragma once

#include "EASTL/fixed_vector.h"
#include "EASTL/optional.h"

#include "Enums.hpp"
#include "Handles.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"

namespace KryneEngine
{
    struct RenderPassDesc
    {
        struct Attachment
        {
            enum class LoadOperation: u8
            {
                Load,
                Clear,
                DontCare,
            };

            enum class StoreOperation: u8
            {
                Store,
                Resolve,
                DontCare
            };

            LoadOperation m_loadOperation = LoadOperation::DontCare;
            StoreOperation m_storeOperation = StoreOperation::DontCare;
            TextureLayout m_initialLayout = TextureLayout::Unknown;
            TextureLayout m_finalLayout = TextureLayout::ColorAttachment;
            RenderTargetViewHandle m_rtv;
            float4 m_clearColor = float4(0, 0, 0, 0);
        };

        struct DepthStencilAttachment: public Attachment
        {
            LoadOperation m_stencilLoadOperation = LoadOperation::DontCare;
            StoreOperation m_stencilStoreOperation = StoreOperation::DontCare;
            u8 m_stencilClearValue = 0;
            bool m_readOnly = false;
        };

        static constexpr u8 kMaxSupportedColorAttachments = 8;
        eastl::fixed_vector<Attachment, kMaxSupportedColorAttachments, false> m_colorAttachments {};
        eastl::optional<DepthStencilAttachment> m_depthStencilAttachment {};

#if !defined(KE_FINAL)
        eastl::string m_debugName;
#endif
    };
}