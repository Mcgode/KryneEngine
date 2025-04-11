/**
 * @file
 * @author Max Godefroy
 * @date 11/03/2023.
 */

#pragma once

#include <comdef.h>

#include "Graphics/DirectX12/Dx12Headers.hpp"
#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Graphics/Enums.hpp"
#include "KryneEngine/Core/Graphics/GraphicsCommon.hpp"
#include "KryneEngine/Core/Graphics/MemoryBarriers.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "KryneEngine/Core/Graphics/Texture.hpp"

namespace KryneEngine
{
#define Dx12Verify(condition) KE_VERIFY(SUCCEEDED(condition))

    inline void Dx12Assert(HRESULT _hr)
    {
        if (!SUCCEEDED(_hr))
        {
            _com_error e(_hr);
            KE_FATAL(e.ErrorMessage());
        }
    }

    template <class T>
    inline void SafeRelease(T& _pointer)
    {
        if (_pointer != nullptr) [[likely]]
        {
            _pointer->Release();
            _pointer = nullptr;
        }
    }

    template <class T>
    inline void SafeRelease(ComPtr<T>& _pointer)
    {
        _pointer = nullptr;
    }

    template <class DxObject, class... Args>
    void Dx12SetName(DxObject* _object, const wchar_t* _format, Args... _args)
    {
        eastl::wstring name;
        name.sprintf(_format, _args...);
        Dx12Assert(_object->SetPrivateData(WKPDID_D3DDebugObjectNameW, name.size() * sizeof(wchar_t), name.c_str()));
    }

    template <class DxObject, class... Args>
    void Dx12SetName(DxObject* _object, const char* _format, Args... _args)
    {
        eastl::string name;
        name.sprintf(_format, _args...);
        Dx12Assert(_object->SetPrivateData(WKPDID_D3DDebugObjectName, name.size() * sizeof(char), name.c_str()));
    }

    namespace Dx12Converters
    {
        [[nodiscard]] inline D3D_FEATURE_LEVEL GetFeatureLevel(const GraphicsCommon::ApplicationInfo& _appInfo)
        {
        	KE_ASSERT(_appInfo.IsDirectX12Api());

            switch (_appInfo.m_api)
            {
                case GraphicsCommon::Api::DirectX12_2:
                    return D3D_FEATURE_LEVEL_12_2;
                case GraphicsCommon::Api::DirectX12_1:
                    return D3D_FEATURE_LEVEL_12_1;
                default:
                    return D3D_FEATURE_LEVEL_12_0;
            }
        }

        constexpr inline DXGI_FORMAT ToDx12Format(TextureFormat _format)
        {
            DXGI_FORMAT format;

            static_assert(static_cast<u32>(TextureFormat::Count) == 30, "Enum values changed, please update");

#define MAP(commonFormat, dx12Format) case TextureFormat::commonFormat: format = dx12Format; break

            switch (_format)
            {
                MAP(R8_UNorm, DXGI_FORMAT_R8_UNORM);
                MAP(RG8_UNorm, DXGI_FORMAT_R8G8_UNORM);
                MAP(RGB8_UNorm, DXGI_FORMAT_R8G8B8A8_UNORM);
                MAP(RGBA8_UNorm, DXGI_FORMAT_R8G8B8A8_UNORM);

                MAP(RGB8_sRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
                MAP(RGBA8_sRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

                MAP(BGRA8_UNorm, DXGI_FORMAT_B8G8R8A8_UNORM);
                MAP(BGRA8_sRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);

                MAP(R8_SNorm, DXGI_FORMAT_R8_SNORM);
                MAP(RG8_SNorm, DXGI_FORMAT_R8G8_SNORM);
                MAP(RGB8_SNorm, DXGI_FORMAT_R8G8B8A8_SNORM);
                MAP(RGBA8_SNorm, DXGI_FORMAT_R8G8B8A8_SNORM);

                MAP(R16_Float, DXGI_FORMAT_R16_FLOAT);
                MAP(RG16_Float, DXGI_FORMAT_R16G16_FLOAT);
                MAP(RGB16_Float, DXGI_FORMAT_R16G16B16A16_FLOAT);
                MAP(RGBA16_Float, DXGI_FORMAT_R16G16B16A16_FLOAT);

                MAP(R32_Float, DXGI_FORMAT_R32_FLOAT);
                MAP(RG32_Float, DXGI_FORMAT_R32G32_FLOAT);
                MAP(RGB32_Float, DXGI_FORMAT_R32G32B32_FLOAT);
                MAP(RGBA32_Float, DXGI_FORMAT_R32G32B32A32_FLOAT);

                MAP(R32_UInt, DXGI_FORMAT_R32_UINT);
                MAP(RG32_UInt, DXGI_FORMAT_R32G32_UINT);
                MAP(RGB32_UInt, DXGI_FORMAT_R32G32B32_UINT);
                MAP(RGBA32_UInt, DXGI_FORMAT_R32G32B32A32_UINT);

                MAP(D16, DXGI_FORMAT_D16_UNORM);
                MAP(D24, DXGI_FORMAT_D24_UNORM_S8_UINT);
                MAP(D32F, DXGI_FORMAT_D32_FLOAT);
                MAP(D24S8, DXGI_FORMAT_D24_UNORM_S8_UINT);
                MAP(D32FS8, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
                default:
                    KE_ASSERT_MSG(_format == TextureFormat::NoFormat, "Unknown format");
                    format = DXGI_FORMAT_UNKNOWN;
            }

#undef MAP

            return format;
        }

        constexpr inline DXGI_FORMAT ToDx12SrvFormat(TextureFormat _format)
        {
            DXGI_FORMAT format;

#define MAP(commonFormat, dx12Format) case TextureFormat::commonFormat: format = dx12Format; break

            switch (_format)
            {
                MAP(R8_UNorm, DXGI_FORMAT_R8_UNORM);
                MAP(RG8_UNorm, DXGI_FORMAT_R8G8_UNORM);
                MAP(RGB8_UNorm, DXGI_FORMAT_R8G8B8A8_UNORM);
                MAP(RGBA8_UNorm, DXGI_FORMAT_R8G8B8A8_UNORM);

                MAP(RGB8_sRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
                MAP(RGBA8_sRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

                MAP(BGRA8_UNorm, DXGI_FORMAT_B8G8R8A8_UNORM);
                MAP(BGRA8_sRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);

                MAP(R8_SNorm, DXGI_FORMAT_R8_SNORM);
                MAP(RG8_SNorm, DXGI_FORMAT_R8G8_SNORM);
                MAP(RGB8_SNorm, DXGI_FORMAT_R8G8B8A8_SNORM);
                MAP(RGBA8_SNorm, DXGI_FORMAT_R8G8B8A8_SNORM);

                MAP(R32_Float, DXGI_FORMAT_R32_FLOAT);
                MAP(RG32_Float, DXGI_FORMAT_R32G32_FLOAT);
                MAP(RGB32_Float, DXGI_FORMAT_R32G32B32_FLOAT);
                MAP(RGBA32_Float, DXGI_FORMAT_R32G32B32A32_FLOAT);

                MAP(D16, DXGI_FORMAT_R16_UNORM);
                MAP(D24, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
                MAP(D32F, DXGI_FORMAT_R32_FLOAT);
                MAP(D24S8, DXGI_FORMAT_R24G8_TYPELESS);
                MAP(D32FS8, DXGI_FORMAT_R32G8X24_TYPELESS);
                default:
                    KE_ASSERT_MSG(_format == TextureFormat::NoFormat, "Unknown format");
                    format = DXGI_FORMAT_UNKNOWN;
            }

#undef MAP

            return format;
        }

        constexpr inline TextureFormat FromDx12Format(DXGI_FORMAT _format)
        {
            TextureFormat format;

            static_assert(static_cast<u32>(TextureFormat::Count) == 30, "Enum values changed, please update");

#define MAP(commonFormat, dx12Format) case dx12Format: format = TextureFormat::commonFormat; break

            switch (_format)
            {
                MAP(R8_UNorm, DXGI_FORMAT_R8_UNORM);
                MAP(RG8_UNorm, DXGI_FORMAT_R8G8_UNORM);
                // MAP(RGB8_UNorm, DXGI_FORMAT_R8G8B8A8_UNORM);
                MAP(RGBA8_UNorm, DXGI_FORMAT_R8G8B8A8_UNORM);

                // MAP(RGB8_sRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
                MAP(RGBA8_sRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

                MAP(BGRA8_UNorm, DXGI_FORMAT_B8G8R8A8_UNORM);
                MAP(BGRA8_sRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);

                MAP(R8_SNorm, DXGI_FORMAT_R8_SNORM);
                MAP(RG8_SNorm, DXGI_FORMAT_R8G8_SNORM);
                // MAP(RGB8_SNorm, DXGI_FORMAT_R8G8B8A8_SNORM);
                MAP(RGBA8_SNorm, DXGI_FORMAT_R8G8B8A8_SNORM);

                MAP(R16_Float, DXGI_FORMAT_R16_FLOAT);
                MAP(RG16_Float, DXGI_FORMAT_R16G16_FLOAT);
                // MAP(RGB16_Float, DXGI_FORMAT_R16G16B16_FLOAT);
                MAP(RGBA16_Float, DXGI_FORMAT_R16G16B16A16_FLOAT);

                MAP(R32_Float, DXGI_FORMAT_R32_FLOAT);
                MAP(RG32_Float, DXGI_FORMAT_R32G32_FLOAT);
                MAP(RGB32_Float, DXGI_FORMAT_R32G32B32_FLOAT);
                MAP(RGBA32_Float, DXGI_FORMAT_R32G32B32A32_FLOAT);

                MAP(R32_UInt, DXGI_FORMAT_R32_UINT);
                MAP(RG32_UInt, DXGI_FORMAT_R32G32_UINT);
                MAP(RGB32_UInt, DXGI_FORMAT_R32G32B32_UINT);
                MAP(RGBA32_UInt, DXGI_FORMAT_R32G32B32A32_UINT);

                MAP(D16, DXGI_FORMAT_D16_UNORM);
                // MAP(D24, DXGI_FORMAT_D24_UNORM_S8_UINT);
                MAP(D32F, DXGI_FORMAT_D32_FLOAT);
                MAP(D24S8, DXGI_FORMAT_D24_UNORM_S8_UINT);
                MAP(D32FS8, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
                default:
                    KE_ASSERT_MSG(_format != DXGI_FORMAT_UNKNOWN, "Unknown format");
                    format = TextureFormat::NoFormat;
            }

#undef MAP

            return format;
        }

        constexpr inline D3D12_RESOURCE_STATES ToDx12ResourceState(TextureLayout _layout)
        {
            D3D12_RESOURCE_STATES result;

#define MAP(commonLayout, dx12State) case TextureLayout::commonLayout: result = dx12State; break
            switch (_layout) {
                MAP(Unknown, D3D12_RESOURCE_STATE_COMMON);
                MAP(Common, D3D12_RESOURCE_STATE_COMMON);
                MAP(Present, D3D12_RESOURCE_STATE_PRESENT);
                MAP(GenericRead, D3D12_RESOURCE_STATE_GENERIC_READ);
                MAP(ColorAttachment, D3D12_RESOURCE_STATE_RENDER_TARGET);
                MAP(DepthStencilAttachment, D3D12_RESOURCE_STATE_DEPTH_WRITE);
                MAP(DepthStencilReadOnly, D3D12_RESOURCE_STATE_DEPTH_READ);
                MAP(UnorderedAccess, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                MAP(ShaderResource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
                MAP(TransferSrc, D3D12_RESOURCE_STATE_COPY_SOURCE);
                MAP(TransferDst, D3D12_RESOURCE_STATE_COPY_DEST);
                MAP(ResolveSrc, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
                MAP(ResolveDst, D3D12_RESOURCE_STATE_RESOLVE_DEST);
                MAP(ShadingRate, D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE);
            }
#undef MAP

            return result;
        }

        [[nodiscard]] inline constexpr D3D12_RESOURCE_DIMENSION GetTextureResourceDimension(TextureTypes _type)
        {
            D3D12_RESOURCE_DIMENSION result = D3D12_RESOURCE_DIMENSION_UNKNOWN;

#define MAP(commonType, dx12Dimension) case TextureTypes::commonType: result = dx12Dimension; break
            switch (_type)
            {
                MAP(Single1D, D3D12_RESOURCE_DIMENSION_TEXTURE1D);
                MAP(Single2D, D3D12_RESOURCE_DIMENSION_TEXTURE2D);
                MAP(Single3D, D3D12_RESOURCE_DIMENSION_TEXTURE3D);
                MAP(SingleCube, D3D12_RESOURCE_DIMENSION_TEXTURE2D);
                MAP(Array1D, D3D12_RESOURCE_DIMENSION_TEXTURE1D);
                MAP(Array2D, D3D12_RESOURCE_DIMENSION_TEXTURE2D);
                MAP(ArrayCube, D3D12_RESOURCE_DIMENSION_TEXTURE2D);
            default:
                KE_ERROR("Unreachable code");
            }
#undef MAP
            return result;
        }

        [[nodiscard]] inline constexpr D3D12_RESOURCE_FLAGS GetTextureResourceFlags(MemoryUsage _usage)
        {
            D3D12_RESOURCE_FLAGS result = D3D12_RESOURCE_FLAG_NONE;
            if (BitUtils::EnumHasAll(_usage, MemoryUsage::ColorTargetImage))
            {
                result |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            }
            if (BitUtils::EnumHasAll(_usage, MemoryUsage::DepthStencilTargetImage))
            {
                result |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            }
            if (BitUtils::EnumHasAll(_usage, MemoryUsage::WriteImage))
            {
                result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            }
            if (!BitUtils::EnumHasAny(_usage, MemoryUsage::ReadImage | MemoryUsage::SampledImage)
                && BitUtils::EnumHasAny(_usage, MemoryUsage::DepthStencilTargetImage))
            {
                result |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
            }
            return result;
        }

        [[nodiscard]] inline constexpr D3D12_HEAP_TYPE GetHeapType(MemoryUsage _usage)
        {
            switch (_usage & MemoryUsage::USAGE_TYPE_MASK)
            {
            case MemoryUsage::GpuOnly_UsageType:
                return D3D12_HEAP_TYPE_DEFAULT;
            case MemoryUsage::StageOnce_UsageType:
            case MemoryUsage::StageEveryFrame_UsageType:
                return D3D12_HEAP_TYPE_UPLOAD;
            case MemoryUsage::Readback_UsageType:
                return D3D12_HEAP_TYPE_READBACK;
            default:
                KE_ERROR("Unsupported memory usage type");
                return D3D12_HEAP_TYPE_DEFAULT;
            }
        }

        inline u32 RetrievePlaneSlice(TexturePlane _planes, TexturePlane _selectedPlane)
        {
            if (BitUtils::EnumHasAll(_planes, TexturePlane::Depth | TexturePlane::Stencil))
            {
                return _selectedPlane == TexturePlane::Depth ? 0 : 1;
            }
            return 0;
        }

        [[nodiscard]] D3D12_TEXTURE_ADDRESS_MODE ToDx12AddressMode(SamplerDesc::AddressMode _addressMode);

        [[nodiscard]] D3D12_BARRIER_SYNC ToDx12BarrierSync(BarrierSyncStageFlags _flags);
        [[nodiscard]] D3D12_BARRIER_ACCESS ToDx12BarrierAccess(BarrierAccessFlags _flags);
        [[nodiscard]] D3D12_BARRIER_LAYOUT ToDx12BarrierLayout(TextureLayout _layout);
        [[nodiscard]] D3D12_RESOURCE_STATES RetrieveState(BarrierAccessFlags _access, TextureLayout _layout);

        [[nodiscard]] D3D12_SHADER_VISIBILITY ToDx12ShaderVisibility(ShaderVisibility _visibility);

        [[nodiscard]] D3D12_BLEND ToDx12Blend(ColorAttachmentBlendDesc::BlendFactor _blendFactor);
        [[nodiscard]] D3D12_BLEND_OP ToDx12BlendOp(ColorAttachmentBlendDesc::BlendOp _blendOp);
        [[nodiscard]] D3D12_LOGIC_OP ToDx12LogicOp(ColorBlendingDesc::LogicOp _logicOp);
        [[nodiscard]] D3D12_COMPARISON_FUNC ToDx12CompareFunc(DepthStencilStateDesc::CompareOp _compareOp);
        [[nodiscard]] D3D12_STENCIL_OP ToDx12StencilOp(DepthStencilStateDesc::StencilOp _stencilOp);
        [[nodiscard]] const char* ToDx12SemanticName(VertexLayoutElement::SemanticName _semanticName);

        [[nodiscard]] D3D_PRIMITIVE_TOPOLOGY ToDx12Topology(InputAssemblyDesc::PrimitiveTopology _topology);
    }

    u8 GetTextureBytesPerPixel(DXGI_FORMAT _format);

    void DebugLayerMessageCallback(
            D3D12_MESSAGE_CATEGORY _category,
            D3D12_MESSAGE_SEVERITY _severity,
            D3D12_MESSAGE_ID _id,
            LPCSTR _description,
            void* _context);
}