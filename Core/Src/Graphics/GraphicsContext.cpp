/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2022.
 */

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"

#include "KryneEngine/Core/Graphics/EnumHelpers.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp"
#include "KryneEngine/Core/Profiling/TracyGpuProfilerContext.hpp"
#include "KryneEngine/Core/Window/Window.hpp"

#if defined(KE_GRAPHICS_API_VK)
#   include "Graphics/Vulkan/VkGraphicsContext.hpp"
#elif defined(KE_GRAPHICS_API_DX12)
#   include "Graphics/DirectX12/Dx12GraphicsContext.hpp"
#elif defined(KE_GRAPHICS_API_MTL)
#   include "Graphics/Metal/MetalGraphicsContext.hpp"
#else
#   error No valid graphics API
#endif

namespace KryneEngine
{
    using Implementation =
#if defined(KE_GRAPHICS_API_VK)
        VkGraphicsContext;
#elif defined(KE_GRAPHICS_API_DX12)
        Dx12GraphicsContext;
#elif defined(KE_GRAPHICS_API_MTL)
        MetalGraphicsContext;
#endif

    GraphicsContext* GraphicsContext::Create(
        const GraphicsCommon::ApplicationInfo& _appInfo,
        Window* _window,
        AllocatorInstance _allocator)
    {
        auto* context = _allocator.New<Implementation>(_allocator, _appInfo, _window);
#if defined(TRACY_ENABLE)
        context->m_profilerContext = context->m_allocator.New<TracyGpuProfilerContext>(
            _allocator, context->GetFrameContextCount());
#endif
        return context;
    }
    void GraphicsContext::Destroy(GraphicsContext* _context)
    {
        if (_context->m_profilerContext != nullptr)
        {
            _context->m_allocator.Delete(_context->m_profilerContext);
        }
        _context->m_allocator.Delete(reinterpret_cast<Implementation*>(_context));;
    }

    bool GraphicsContext::EndFrame()
    {
        InternalEndFrame();
        m_frameId++;
        if (m_window == nullptr)
        {
            return false;
        }
        else
        {
            return m_window->WaitForEvents();
        }
    }

    const char* GraphicsContext::GetShaderFileExtension()
    {
#if defined(KE_GRAPHICS_API_VK)
        return "spv";
#elif defined(KE_GRAPHICS_API_DX12)
        return "cso";
#elif defined(KE_GRAPHICS_API_MTL)
        return "metallib";
#else
        static_assert("Not yet implemented");
        return nullptr;
#endif
    }

    GraphicsContext::GraphicsContext(
        AllocatorInstance _allocator,
        const GraphicsCommon::ApplicationInfo& _appInfo,
        Window* _window)
        : m_appInfo(_appInfo)
        , m_window(_window)
        , m_allocator(_allocator)
        , m_frameId(kInitialFrameId)
    {}

    TextureHandle GraphicsContext::CreateTexture(const TextureCreateDesc& _createDesc)
    {
        if (!KE_VERIFY_MSG(
                (_createDesc.m_memoryUsage & MemoryUsage::USAGE_TYPE_MASK) == MemoryUsage::GpuOnly_UsageType,
                "The engine is designed around having buffers representing textures on the CPU")) [[unlikely]]
        {
            return { GenPool::kInvalidHandle };
        }

        VERIFY_OR_RETURN(
            _createDesc.m_desc.m_dimensions.x != 0
                && _createDesc.m_desc.m_dimensions.y != 0
                && _createDesc.m_desc.m_dimensions.z != 0
                && _createDesc.m_desc.m_arraySize != 0
                && _createDesc.m_desc.m_mipCount != 0,
            { GenPool::kInvalidHandle });

        VERIFY_OR_RETURN(BitUtils::EnumHasAny(_createDesc.m_memoryUsage, ~MemoryUsage::USAGE_TYPE_MASK),
                         { GenPool::kInvalidHandle });

        VERIFY_OR_RETURN(
            !(BitUtils::EnumHasAny(_createDesc.m_memoryUsage, MemoryUsage::DepthStencilTargetImage)
                ^ GraphicsEnumHelpers::IsDepthOrStencilFormat(_createDesc.m_desc.m_format)),
            { GenPool::kInvalidHandle });

        return { GenPool::kUndefinedHandle };
    }

    TextureViewHandle GraphicsContext::CreateTextureView(const TextureViewDesc& _viewDesc)
    {
        KE_ASSERT_MSG(!BitUtils::EnumHasAny(_viewDesc.m_accessType, TextureViewAccessType::Write)
                      || memcmp(
                         _viewDesc.m_componentsMapping.data(),
                         kDefaultTexture4ComponentsMapping.data(),
                         sizeof(Texture4ComponentsMapping)) == 0,
                      "Component remapping is not supported for write access");
        return { GenPool::kUndefinedHandle };
    }

    bool GraphicsContext::SupportsNonGlobalBarriers()
    {
#if defined(KE_GRAPHICS_API_VK)
        return true;
#elif defined(KE_GRAPHICS_API_DX12)
        return true;
#elif defined(KE_GRAPHICS_API_MTL)
        return false;
#else
#   error Unsupported graphics API
#endif
    }

    bool GraphicsContext::RenderPassNeedsUsageDeclaration()
    {
#if defined(KE_GRAPHICS_API_VK)
        return false;
#elif defined(KE_GRAPHICS_API_DX12)
        return false;
#elif defined(KE_GRAPHICS_API_MTL)
        return true;
#else
#   error Unsupported graphics API
#endif
    }

    bool GraphicsContext::ComputePassNeedsUsageDeclaration()
    {
#if defined(KE_GRAPHICS_API_VK)
        return false;
#elif defined(KE_GRAPHICS_API_DX12)
        return false;
#elif defined(KE_GRAPHICS_API_MTL)
        return true;
#else
#   error Unsupported graphics API
#endif
    }
}
