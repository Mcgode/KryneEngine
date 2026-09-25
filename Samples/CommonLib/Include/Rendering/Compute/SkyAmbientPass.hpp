/**
 * @file
 * @author Max Godefroy
 * @date 18/09/2026.
 */

#pragma once

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/BufferView.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp"
#include "KryneEngine/Modules/RenderGraph/RenderGraph.hpp"

namespace KryneEngine::Samples
{
    /**
     * @brief Compute pass that bakes sky (and ground) radiance into a 2nd-order spherical-
     * harmonics irradiance map each frame.
     *
     * @details
     * Dispatches a single 128-thread workgroup that samples the atmosphere shader over the full
     * sphere and projects the result onto a 9-coefficient SH basis, writing it to a small GPU
     * buffer. The buffer is then consumed by DeferredShadingPass for the ambient term, giving a
     * cheap but directional (not just up/down) approximation of sky lighting.
     *
     * Output buffer layout (9 × float4, one per SH coefficient, colour in .rgb). See
     * `SphericalHarmonics.hlsli` for the coefficient ordering and reconstruction.
     */
    class SkyAmbientPass
    {
    public:
        explicit SkyAmbientPass(AllocatorInstance _allocator);

        void Initialize(GraphicsContext* _graphicsContext);

        void UpdateSceneConstants(GraphicsContext* _graphicsContext, BufferViewHandle _sceneConstantsBufferView) const;

        void Dispatch(
            const Modules::RenderGraph::RenderGraph& _renderGraph,
            const Modules::RenderGraph::PassExecutionData& _passExecutionData) const;

        void CreatePso(GraphicsContext* _graphicsContext);

        [[nodiscard]] BufferHandle GetSkyAmbientBuffer() const { return m_skyAmbientBuffer; }
        [[nodiscard]] BufferViewHandle GetSkyAmbientBufferView() const { return m_skyAmbientBufferView; }

    private:
        AllocatorInstance m_allocator;

        DescriptorSetLayoutHandle m_descriptorSetLayout {};
        DescriptorSetHandle m_descriptorSet {};

        struct Indices
        {
            u32 m_sceneConstants;
            u32 m_skyAmbient;

            [[nodiscard]] u32* Get() { return &m_sceneConstants; }
        } m_indices {};

        BufferHandle m_skyAmbientBuffer {};
        BufferViewHandle m_skyAmbientBufferView {};

        PipelineLayoutHandle m_pipelineLayout {};
        ComputePipelineHandle m_pso {};
    };
}
