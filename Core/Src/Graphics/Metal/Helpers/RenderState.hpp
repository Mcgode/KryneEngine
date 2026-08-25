/**
 * @file
 * @author Max Godefroy
 * @date 05/11/2024.
 */

#pragma once

#include <Metal/MTL4ArgumentTable.hpp>
#include "KryneEngine/Core/Graphics/Buffer.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "NsPtr.hpp"


namespace KryneEngine
{
    // Default values match the default values from the Metal doc
    struct RenderDynamicState
    {
        float4 m_blendFactor { 0.f, 0.f, 0.f, 0.f };

        u64 m_depthStencilHash = 0;

        float m_depthBias = 0.f;
        float m_depthBiasSlope = 0.f;

        float m_depthBiasClamp = 0.f;
        RasterStateDesc::FillMode m_fillMode = RasterStateDesc::FillMode::Solid;
        RasterStateDesc::CullMode m_cullMode = RasterStateDesc::CullMode::None;
        RasterStateDesc::Front m_front : 7 = RasterStateDesc::Front::Clockwise;
        bool m_depthClip : 1 = true;
        u8 m_stencilRefValue = 0;
    };

    struct RenderState
    {
        RenderDynamicState m_dynamicState {};
        BufferSpan m_indexBufferView {};
        bool m_indexBufferIsU16 = false;
        InputAssemblyDesc::PrimitiveTopology m_topology {};
        NsPtr<MTL4::ArgumentTable> m_vertexArgumentTable {};
        NsPtr<MTL4::ArgumentTable> m_fragmentArgumentTable {};
    };
}