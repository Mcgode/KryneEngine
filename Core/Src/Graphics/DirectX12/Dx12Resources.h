/**
 * @file
 * @author Max Godefroy
 * @date 23/02/2024.
 */

#pragma once

#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Graphics/RenderPass.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "KryneEngine/Core/Graphics/Texture.hpp"
#include "KryneEngine/Core/Memory/DynamicArray.hpp"
#include "KryneEngine/Core/Memory/IndexAllocator.hpp"

namespace D3D12MA
{
    class Allocator;
    class Allocation;
}

namespace KryneEngine
{
    struct BufferViewDesc;
    struct BufferCreateDesc;
    struct TextureViewDesc;
    struct RenderTargetViewDesc;

    class Dx12FrameContext;
    class Dx12DescriptorSetManager;

    class Dx12Resources
    {
        friend class Dx12GraphicsContext;
        friend class Dx12DescriptorSetManager;

    public:
        Dx12Resources(AllocatorInstance _allocator);
        ~Dx12Resources();

        void InitAllocator(ID3D12Device* _device, IDXGIAdapter* _adapter);
        void InitHeaps(ID3D12Device* _device);

        void FlushPools();

        [[nodiscard]] BufferHandle CreateBuffer(const BufferCreateDesc& _desc);
        [[nodiscard]] BufferHandle CreateStagingBuffer(
            const TextureDesc& _desc,
            const eastl::span<const TextureMemoryFootprint>& _footprints);
        bool DestroyBuffer(BufferHandle _buffer);

        [[nodiscard]] TextureHandle CreateTexture(const TextureCreateDesc& _createDesc, ID3D12Device* _device);
        [[nodiscard]] TextureHandle RegisterTexture(ID3D12Resource* _texture, D3D12MA::Allocation* _allocation = nullptr);
        bool ReleaseTexture(TextureHandle _texture, bool _free);

        [[nodiscard]] TextureViewHandle CreateTextureView(const TextureViewDesc& _viewDesc, ID3D12Device* _device);
        bool DestroyTextureView(TextureViewHandle _textureView);

        [[nodiscard]] SamplerHandle CreateSampler(const SamplerDesc& _samplerDesc, ID3D12Device* _device);
        bool DestroySampler(SamplerHandle _sampler);

        [[nodiscard]] BufferViewHandle CreateBufferView(const BufferViewDesc& _viewDesc, ID3D12Device* _device);
        bool DestroyBufferView(BufferViewHandle _bufferCbv);

        [[nodiscard]] RenderTargetViewHandle CreateRenderTargetView(const RenderTargetViewDesc& _desc, ID3D12Device* _device);
        bool FreeRenderTargetView(RenderTargetViewHandle _rtv);

        [[nodiscard]] RenderPassHandle CreateRenderPass(const RenderPassDesc& _desc);
        bool FreeRenderPass(RenderPassHandle _handle);

        [[nodiscard]] ShaderModuleHandle RegisterShaderModule(void* _bytecodeData, u64 _bytecodeSize);
        bool UnRegisterShaderModule(ShaderModuleHandle _shaderModule);

        [[nodiscard]] PipelineLayoutHandle CreatePipelineLayout(
            const PipelineLayoutDesc& _desc,
            Dx12DescriptorSetManager& _setManager,
            ID3D12Device* _device);
        bool DestroyPipelineLayout(PipelineLayoutHandle _layout);

        [[nodiscard]] GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc, ID3D12Device* _device);
        bool DestroyGraphicsPipeline(GraphicsPipelineHandle _pipeline);

        [[nodiscard]] ComputePipelineHandle CreateComputePipeline(const ComputePipelineDesc& _desc, ID3D12Device* _device);
        bool DestroyComputePipeline(ComputePipelineHandle _pipeline);

        struct RtvHotData
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE m_cpuHandle {};
            TextureHandle m_resource {};
        };

        struct TextureViewHotData
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE m_srvHandle {};
            CD3DX12_CPU_DESCRIPTOR_HANDLE m_uavHandle {};
        };

        struct TextureViewColdData
        {
            u32 m_srvIndex {};
            u32 m_uavIndex {};
        };

        struct BufferViewHotData
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE m_cbvHandle {};
            CD3DX12_CPU_DESCRIPTOR_HANDLE m_srvHandle {};
            CD3DX12_CPU_DESCRIPTOR_HANDLE m_uavHandle {};
        };

        struct BufferViewColdData
        {
            u32 m_cbvIndex{};
            u32 m_srvIndex {};
            u32 m_uavIndex {};
        };

        struct PsoColdData
        {
            ID3D12RootSignature* m_signature;
            InputAssemblyDesc::PrimitiveTopology m_topology;
        };

        struct PipelineLayoutHotData
        {
            ID3D12RootSignature* m_signature;
            u16* m_tableSetOffsets;
        };

        GenerationalPool<ID3D12Resource*, D3D12MA::Allocation*> m_buffers;
        GenerationalPool<ID3D12Resource*, D3D12MA::Allocation*> m_textures;
        IndexAllocator m_cbvSrvUavAllocator;
        GenerationalPool<CD3DX12_CPU_DESCRIPTOR_HANDLE> m_samplers;
        GenerationalPool<TextureViewHotData, TextureViewColdData> m_textureViews;
        GenerationalPool<BufferViewHotData, BufferViewColdData> m_bufferViews;
        GenerationalPool<RtvHotData, DXGI_FORMAT> m_renderTargetViews;
        GenerationalPool<RtvHotData, DXGI_FORMAT> m_depthStencilViews;
        GenerationalPool<RenderPassDesc> m_renderPasses;
        GenerationalPool<PipelineLayoutHotData, u32> m_pipelineLayouts;
        GenerationalPool<D3D12_SHADER_BYTECODE> m_shaderBytecodes;
        GenerationalPool<ID3D12PipelineState*, PsoColdData> m_pipelineStateObjects;

    private:
        static constexpr u16 kRtvHeapSize = 2048;
        ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap = nullptr;
        u32 m_rtvDescriptorSize = 0;

        static constexpr u16 kDsvHeapSize = 256;
        ComPtr<ID3D12DescriptorHeap> m_dsvDescriptorHeap = nullptr;
        u32 m_dsvDescriptorSize = 0;

        static constexpr u32 kDsvFlag = 1u << (GenPool::kIndexBits - 1u);

        static_assert(GenPool::kIndexBits == 20, "GenPool index type changed, please update size appropriately.");
        static constexpr u64 kCbvSrvUavHeapSize = 1u << 16;
        ComPtr<ID3D12DescriptorHeap> m_cbvSrvUavDescriptorStorageHeap;
        u32 m_cbvSrvUavDescriptorSize = 0;

        static constexpr u16 kSamplerHeapSize = 512;
        ComPtr<ID3D12DescriptorHeap> m_samplerStorageHeap = nullptr;
        u32 m_samplerDescriptorSize = 0;

        D3D12MA::Allocator* m_memoryAllocator = nullptr;
    };
} // KryneEngine