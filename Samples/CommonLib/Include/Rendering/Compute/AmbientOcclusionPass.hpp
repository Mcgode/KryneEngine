/**
 * @file
 * @author Max Godefroy
 * @date 25/09/2026.
 */

#pragma once

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Memory/SimplePool.hpp>
#include <KryneEngine/Modules/GraphicsUtils/DynamicBuffer.hpp>
#include <KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp>

namespace KryneEngine::Modules::RenderGraph
{
    class Builder;
}

namespace KryneEngine::Samples
{
    /**
     * @brief Screen-space ambient occlusion, adapted from Intel's XeGTAO (ground-truth horizon-
     * based AO; Jimenez et al., "Practical Real-Time Strategies for Accurate Indirect Occlusion").
     *
     * @details
     * Runs three compute dispatches per frame as a single merged render graph pass (see
     * #DeclareRenderGraphPass): a main dispatch reads the GBuffer depth/normal and, for each
     * pixel, sweeps a small number of view-aligned slices, marching a few steps outward along
     * each to find the horizon angle in both directions, then integrates the resulting visible
     * arc - the occlusion estimate - plus a packed depth-edges byte used only by the denoiser. A
     * denoise dispatch then runs an edge-aware spatial blur over that raw term twice,
     * ping-ponging between two working textures, to avoid smearing across depth discontinuities.
     *
     * Transitions between the three internal dispatches are placed by hand (in #Dispatch) rather
     * than relying on the render graph's automatic per-pass barriers, since those only run
     * between whole declared passes. #GetFinalOutputView is always the same resource regardless
     * of how many denoise passes ran: a single R8_UNorm channel, 1 = fully visible, 0 = fully
     * occluded, meant to multiply whichever ambient/indirect lighting term the caller adds in,
     * never the direct sun term.
     *
     * Simplified from XeGTAO's reference HLSL
     * (https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.hlsli):
     * no depth mip-chain prefilter (samples GBuffer depth directly), no bent-normal output, no
     * half-float packing, and a per-pixel (rather than 2-pixels-per-thread) denoiser.
     *
     * The raw textures/views passed into #Initialize are expected to already be registered into
     * the render graph's registry by the caller; this class only owns the compute pipelines and
     * its own tunables constant buffer.
     */
    class AmbientOcclusionPass
    {
    public:
        explicit AmbientOcclusionPass(AllocatorInstance _allocator);
        ~AmbientOcclusionPass();

        void Initialize(
            GraphicsContext* _graphicsContext,
            TextureViewHandle _gBufferDepth,
            TextureViewHandle _gBufferNormal,
            TextureViewHandle _aoTermA,
            TextureViewHandle _aoTermB,
            TextureViewHandle _aoEdges,
            TextureHandle _aoTermATexture,
            TextureHandle _aoTermBTexture,
            TextureHandle _aoEdgesTexture);

        void CreatePso(GraphicsContext* _graphicsContext);

        void UpdateSceneConstants(GraphicsContext* _graphicsContext, BufferViewHandle _fullscreenConstantsBufferView) const;

        /**
         * @brief Declares the single merged render graph compute pass that runs the main
         * dispatch and the two denoise dispatches. The resource handles are the render graph's
         * own SimplePoolHandles, as registered by the caller.
         *
         * @note The three dispatches are kept in one pass, with the constants upload running as
         * that pass's own pre-pass transfer function, because the render graph only guarantees
         * callback ordering within a single pass. Splitting them across separately-scheduled
         * passes would let the constants upload's descriptor set writes race against another
         * pass's dispatch binding those same descriptor sets.
         */
        void DeclareRenderGraphPass(
            Modules::RenderGraph::Builder& _builder,
            SimplePoolHandle _gBufferDepthView,
            SimplePoolHandle _gBuffer1View,
            SimplePoolHandle _aoTermAView,
            SimplePoolHandle _aoTermBView,
            SimplePoolHandle _aoEdgesView,
            uint2 _renderSize);

        void Debug(bool* _windowOpen = nullptr);

        [[nodiscard]] TextureViewHandle GetFinalOutputView() const { return m_aoTermA; }

    private:
        // Uploads this frame's tunables to the GPU constants buffer and rebinds it into the
        // main/denoise descriptor sets; runs once per frame with an already-open transfer encoder.
        void UpdateConstants(GraphicsContext* _graphicsContext, TransferCommandEncoderHandle _transferEncoder);

        void Dispatch(
            const Modules::RenderGraph::RenderGraph& _renderGraph,
            const Modules::RenderGraph::PassExecutionData& _passExecutionData,
            uint2 _renderSize) const;

        void DispatchMain(
            const Modules::RenderGraph::RenderGraph& _renderGraph,
            const Modules::RenderGraph::PassExecutionData& _passExecutionData,
            uint2 _renderSize) const;

        // _passIndex 0 reads aoTermA/writes aoTermB; 1 reads aoTermB/writes aoTermA - so the final
        // result always ends up in aoTermA, i.e. #GetFinalOutputView, regardless of which frame.
        void DispatchDenoise(
            const Modules::RenderGraph::RenderGraph& _renderGraph,
            const Modules::RenderGraph::PassExecutionData& _passExecutionData,
            uint2 _renderSize,
            u32 _passIndex) const;

        AllocatorInstance m_allocator;

        float m_effectRadius = 0.5f;
        float m_falloffRangeFraction = 0.615f;
        float m_sampleDistributionPower = 2.f;
        float m_thinOccluderCompensation = 0.f;
        float m_finalValuePower = 2.2f;
        float m_denoiseBlurBeta = 1.2f;
        float m_intensity = 1.f;
        s32 m_sliceCount = 3;
        s32 m_stepsPerSlice = 3;

        TextureViewHandle m_aoTermA {};
        TextureViewHandle m_aoTermB {};

        // Underlying raw textures backing the three AO views, used to place the manual
        // inter-dispatch barriers in #Dispatch.
        TextureHandle m_aoTermATexture {};
        TextureHandle m_aoTermBTexture {};
        TextureHandle m_aoEdgesTexture {};

        Modules::GraphicsUtils::DynamicBuffer m_constantsBuffer;
        BufferViewHandle* m_constantsBufferViews = nullptr;

        DescriptorSetLayoutHandle m_mainDescriptorSetLayout {};
        DescriptorSetHandle m_mainDescriptorSet {};
        struct MainIndices
        {
            u32 m_fullscreenConstants;
            u32 m_aoConstants;
            u32 m_gBufferDepth;
            u32 m_gBufferNormal;
            u32 m_aoTermOut;
            u32 m_edgesOut;

            [[nodiscard]] u32* Get() { return &m_fullscreenConstants; }
        } m_mainIndices {};
        PipelineLayoutHandle m_mainPipelineLayout {};
        ComputePipelineHandle m_mainPso {};

        DescriptorSetLayoutHandle m_denoiseDescriptorSetLayout {};
        DescriptorSetHandle m_denoiseDescriptorSets[2] {};
        struct DenoiseIndices
        {
            u32 m_aoConstants;
            u32 m_sourceAoTerm;
            u32 m_sourceEdges;
            u32 m_aoTermOut;

            [[nodiscard]] u32* Get() { return &m_aoConstants; }
        } m_denoiseIndices {};
        PipelineLayoutHandle m_denoisePipelineLayout {};
        ComputePipelineHandle m_denoisePso {};
    };
}
