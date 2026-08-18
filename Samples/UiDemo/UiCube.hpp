/**
 * @file
 * @author Max Godefroy
 * @date 14/01/2026.
 */

#pragma once

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Graphics/Handles.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Modules/GraphicsUtils/DynamicBuffer.hpp>
#include <KryneEngine/Modules/GuiLib/Context.hpp>
#include <KryneEngine/Modules/GuiLib/GuiRenderers/BasicGuiRenderer.hpp>
#include <KryneEngine/Modules/TextRendering/Font.hpp>

using namespace KryneEngine;

class UiCube
{
public:
    UiCube(
        AllocatorInstance _allocator,
        GraphicsContext& _graphicsContext,
        Modules::TextRendering::FontManager* _fontManager,
        RenderPassHandle _renderPass,
        Modules::TextRendering::MsdfAtlasManager* _atlasManager);

    void Render(
        GraphicsContext& _graphicsContext,
        CommandListHandle _transferCommandList,
        CommandListHandle _renderCommandList,
        float _contentScale);

private:
    AllocatorInstance m_allocator;

    uint2 m_uiViewportSize { 256, 256 };
    DynamicArray<Modules::GuiLib::Context> m_guiContexts;
    DynamicArray<Modules::GuiLib::BasicGuiRenderer> m_guiRenderers;

    BufferHandle m_vertexBuffer;
    BufferHandle m_indexBuffer;
    BufferHandle m_transferBuffer;
    u32 m_transferFrameId = ~0u;

    Modules::GraphicsUtils::DynamicBuffer m_constantBuffer;
    DynamicArray<BufferViewHandle> m_constantBufferViews;

    DescriptorSetHandle m_descriptorSet;
    u32 m_descriptorSetIndex {};
    PipelineLayoutHandle m_pipelineLayout;
    GraphicsPipelineHandle m_pso;
};
