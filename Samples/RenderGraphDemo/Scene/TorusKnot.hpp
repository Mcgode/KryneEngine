/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2025.
 */

#pragma once

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Math/Matrix.hpp"
#include <KryneEngine/Core/Math/Quaternion.hpp>

namespace KryneEngine::Samples::RenderGraphDemo
{
    class TorusKnot
    {
    public:
        explicit TorusKnot(AllocatorInstance _allocator);

        void BuildPso(
            GraphicsContext* _graphicsContext,
            RenderPassHandle _renderPass,
            DescriptorSetLayoutHandle _descriptorSetLayout);

        void Process(GraphicsContext* _graphicsContext);

        void ProcessTransfers(GraphicsContext* _graphicsContext, CommandListHandle _commandList);

        [[nodiscard]] const float4x4& GetModelMatrix() const { return m_modelMatrix; }
        [[nodiscard]] const float3& GetAlbedo() const { return m_albedo; }
        [[nodiscard]] const float& GetRoughness() const { return m_roughness; }
        [[nodiscard]] const float& GetMetalness() const { return m_metalness; }

        [[nodiscard]] float GetKnotRadius() const { return m_knotRadius; }
        [[nodiscard]] float GetTubeRadius() const { return m_tubeRadius; }
        [[nodiscard]] u32 GetPValue() const { return m_pValue; }
        [[nodiscard]] u32 GetQValue() const { return m_qValue; }

        void RenderGBuffer(
            GraphicsContext* _graphicsContext,
            CommandListHandle _commandList,
            DescriptorSetHandle _sceneConstantsSet);

    private:
        AllocatorInstance m_allocator;
        bool m_meshDirty = true;

        u32 m_radialSegments = 16;
        u32 m_tubularSegments = 64;
        float m_knotRadius = 1.0f;
        float m_tubeRadius = 0.2f;
        u32 m_pValue = 2;
        u32 m_qValue = 3;

        BufferHandle m_vertexBuffer {};
        BufferHandle m_indexBuffer {};
        BufferHandle m_previousVertexBuffer {};
        BufferHandle m_previousIndexBuffer {};

        BufferHandle m_transferBuffer {};
        u64 m_transferFrameId = 0;
        size_t m_indexBufferSize = 0;
        size_t m_vertexBufferSize = 0;

        PipelineLayoutHandle m_pipelineLayout { GenPool::kInvalidHandle };
        GraphicsPipelineHandle m_pso { GenPool::kInvalidHandle };

        float3 m_translation { 0.0f };
        Math::Quaternion m_rotation {};
        float3 m_scale { 1 };
        float4x4 m_modelMatrix {};
        float3 m_albedo { 1.f };
        float m_roughness = 1.f;
        float m_metalness = 0.f;

        bool m_windowOpen = true;

        void RenderWindow();
    };
}
