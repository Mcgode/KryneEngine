/**
 * @file
 * @author Max Godefroy
 * @date 10/08/2024.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"

namespace KryneEngine
{
    struct Viewport
    {
        s32 m_topLeftX = 0;
        s32 m_topLeftY = 0;
        s32 m_width;
        s32 m_height;
        float m_minDepth = 0.f;
        float m_maxDepth = 1.f;
    };

    struct DrawInstancedDesc
    {
        u32 m_vertexCount = 0;
        u32 m_instanceCount = 1;
        u32 m_vertexOffset = 0;
        u32 m_instanceOffset = 0;
    };

    struct DrawIndexedInstancedDesc
    {
        u32 m_elementCount = 0;
        u32 m_instanceCount = 1;
        u32 m_indexOffset = 0;
        s32 m_vertexOffset = 0;
        u32 m_instanceOffset = 0;
    };
}
