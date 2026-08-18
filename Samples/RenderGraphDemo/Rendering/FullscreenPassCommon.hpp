/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2025.
 */

#pragma once

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"

namespace KryneEngine::Samples::RenderGraphDemo::FullscreenPassCommon
{
    [[nodiscard]] extern GraphicsPipelineHandle CreatePso(
        GraphicsContext* _graphicsContext,
        AllocatorInstance _allocator,
        RenderPassHandle _renderPass,
        PipelineLayoutHandle _pipelineLayout,
        const char* _fsShader,
        const char* _fsFunctionName,
        bool _depthTest);

    void Render(
        GraphicsContext* _graphicsContext,
        CommandListHandle _commandList,
        u32 _width,
        u32 _height,
        float _fullscreenDepth,
        GraphicsPipelineHandle _pso,
        PipelineLayoutHandle _pipelineLayout,
        eastl::span<DescriptorSetHandle> _descriptorSets);
}
