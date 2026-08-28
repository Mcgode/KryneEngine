/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2025.
 */

#include "FullscreenPassCommon.hpp"

#include "KryneEngine/Core/Graphics/Drawing.hpp"
#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include <fstream>

namespace KryneEngine::Samples::RenderGraphDemo::FullscreenPassCommon
{
    GraphicsPipelineHandle CreatePso(
        GraphicsContext* _graphicsContext,
        const AllocatorInstance _allocator,
        const RenderTargetSetDesc& _renderTargets,
        const PipelineLayoutHandle _pipelineLayout,
        const char* _fsShader,
        const char* _fsFunctionName,
        const bool _depthTest)
    { 
        void* vsByteCode, *fsByteCode;
        const auto createShaderModule = [&](const auto& _path, void*& _data) -> ShaderModuleHandle
        {
            auto path = eastl::string(_path) + "." + _graphicsContext->GetShaderFileExtension();;
            
            std::ifstream file(path.c_str(), std::ios::binary);
            VERIFY_OR_RETURN(file, { GenPool::kInvalidHandle });

            file.seekg(0, std::ios::end);
            const size_t size = file.tellg();
            _data = _allocator.allocate(size);
            file.seekg(0, std::ios::beg);

            KE_VERIFY(file.read(reinterpret_cast<char*>(_data), size));

            const ShaderModuleHandle handle = _graphicsContext->RegisterShaderModule(_data, size);
            return handle;
        };

        ShaderModuleHandle vsModule = createShaderModule("Shaders/Samples/RenderGraphDemo/FullScreenVS_FullScreenMain", vsByteCode);
        ShaderModuleHandle fsModule = createShaderModule(_fsShader, fsByteCode);

        const ShaderStage stages[] {
            {
                .m_shaderModule = vsModule,
                .m_stage = ShaderStage::Stage::Vertex,
                .m_entryPoint = "FullScreenMain",
            },
            {
                .m_shaderModule = fsModule,
                .m_stage = ShaderStage::Stage::Fragment,
                .m_entryPoint = _fsFunctionName,
            },
        };
        const GraphicsPipelineDesc psoDesc {
            .m_stages = stages,
            .m_colorBlending = { .m_attachments = { ColorAttachmentBlendDesc() } },
            .m_depthStencil = {
                .m_depthTest = _depthTest,
                .m_depthWrite = false,
                .m_depthCompare = DepthStencilStateDesc::CompareOp::GreaterEqual
            },
            .m_renderTargets = _renderTargets,
            .m_pipelineLayout = _pipelineLayout,
#if !defined(KE_FINAL)
            .m_debugName = "DeferredShadingPSO",
#endif
        };

        GraphicsPipelineHandle pso = _graphicsContext->CreateGraphicsPipeline(psoDesc);

        _graphicsContext->FreeShaderModule(fsModule);
        _allocator.deallocate(fsByteCode);
        _graphicsContext->FreeShaderModule(vsModule);
        _allocator.deallocate(vsByteCode);
        
        return pso;
    }

    void Render(
        GraphicsContext* _graphicsContext,
        const RenderCommandEncoderHandle _renderEncoder,
        const u32 _width,
        const u32 _height,
        float _fullscreenDepth,
        const GraphicsPipelineHandle _pso,
        const PipelineLayoutHandle _pipelineLayout,
        eastl::span<DescriptorSetHandle> _descriptorSets)
    {
        _graphicsContext->SetViewport(_renderEncoder, {.m_width = (s32)_width, .m_height = (s32)_height});
        _graphicsContext->SetScissorsRect(
            _renderEncoder, {.m_left = 0, .m_top = 0, .m_right = _width, .m_bottom = _height});
        _graphicsContext->SetGraphicsPipeline(_renderEncoder, _pso);
        _graphicsContext->SetGraphicsDescriptorSets(_renderEncoder, _pipelineLayout, _descriptorSets);

        _graphicsContext->SetGraphicsPushConstant(_renderEncoder, _pipelineLayout, {(u32*)(&_fullscreenDepth), 1}, 0, 0);

        _graphicsContext->DrawInstanced(_renderEncoder, {.m_vertexCount = 3});
    }
} 