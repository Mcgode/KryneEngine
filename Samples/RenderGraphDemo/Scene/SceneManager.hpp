/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2025.
 */

#pragma once

#include "KryneEngine/Modules/ImGui/Context.hpp"
#include <KryneEngine/Core/Memory/UniquePtr.hpp>
#include <KryneEngine/Modules/GraphicsUtils/DynamicBuffer.hpp>
#include <KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp>

namespace KryneEngine::Modules
{
    namespace ImGui
    {
        class Context;
    }

    namespace RenderGraph
    {
        class Builder;
        class Registry;
    }
}

namespace KryneEngine::Samples::RenderGraphDemo
{
    class OrbitCamera;
    class SunLight;
    class TorusKnot;

    class SceneManager
    {
    public:
        explicit SceneManager(
            AllocatorInstance _allocator,
            Window& _window,
            Modules::RenderGraph::Registry& _registry);
        ~SceneManager();

        void PreparePsos(
            GraphicsContext* _graphicsContext,
            RenderPassHandle _dummyGBufferRenderPass);

        void DeclareDataTransferPass(
            const GraphicsContext* _graphicsContext,
            Modules::RenderGraph::Builder& _builder,
            Modules::ImGui::Context* _imGuiContext);

        void Process(GraphicsContext* _graphicsContext);

        void ExecuteTransfers(GraphicsContext* _graphicsContext, CommandListHandle _commandList);

        [[nodiscard]] SimplePoolHandle GetSceneConstantsCbv() const { return m_currentCbv; }
        [[nodiscard]] DescriptorSetLayoutHandle GetDescriptorSetLayout() const { return m_sceneDescriptorSetLayout; }
        [[nodiscard]] DescriptorSetHandle GetSceneDescriptorSet(u8 _index) const { return m_sceneDescriptorSets[_index]; }

        void RenderGBuffer(GraphicsContext* _graphicsContext, CommandListHandle _commandList);

    private:
        AllocatorInstance m_allocator;
        UniquePtr<TorusKnot> m_torusKnot;
        UniquePtr<OrbitCamera> m_orbitCamera;
        UniquePtr<SunLight> m_sunLight;
        uint2 m_windowSize;

        Modules::GraphicsUtils::DynamicBuffer m_sceneConstantsBuffer;
        DynamicArray<BufferViewHandle> m_sceneCbvs;
        eastl::vector<u32> m_sceneDescriptorSetIndices;
        DescriptorSetLayoutHandle m_sceneDescriptorSetLayout;
        DynamicArray<DescriptorSetHandle> m_sceneDescriptorSets;

        DynamicArray<SimplePoolHandle> m_cbRenderGraphHandles;
        DynamicArray<SimplePoolHandle> m_cbvRenderGraphHandles;
        SimplePoolHandle m_currentCbv {};
    };
}
