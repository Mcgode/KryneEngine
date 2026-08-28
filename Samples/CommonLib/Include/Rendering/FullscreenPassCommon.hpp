/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2025.
 */

#pragma once

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"

namespace KryneEngine
{
    struct RenderTargetSetDesc;
}

namespace KryneEngine::Samples::FullscreenPassCommon
{
    [[nodiscard]] extern GraphicsPipelineHandle CreatePso(
        GraphicsContext* _graphicsContext,
        AllocatorInstance _allocator,
        const RenderTargetSetDesc& _renderTargets,
        PipelineLayoutHandle _pipelineLayout,
        const char* _fsShader,
        const char* _fsFunctionName,
        bool _depthTest);

    void Render(
        GraphicsContext* _graphicsContext,
        RenderCommandEncoderHandle _renderEncoder,
        u32 _width,
        u32 _height,
        float _fullscreenDepth,
        GraphicsPipelineHandle _pso,
        PipelineLayoutHandle _pipelineLayout,
        eastl::span<DescriptorSetHandle> _descriptorSets);
}
