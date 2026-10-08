/**
 * @file
 * @author Max Godefroy
 * @date 05/07/2024.
 */

#pragma once

#include "Graphics/DirectX12/Dx12Headers.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"

namespace KryneEngine
{
    struct CommandListSet
    {
        ID3D12GraphicsCommandList7* m_commandList;
        ID3D12CommandAllocator* m_commandAllocator;
        union
        {
            RenderPassHandle m_currentRenderPass;
        };
    };
}
