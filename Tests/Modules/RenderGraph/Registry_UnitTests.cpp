/**
 * @file
 * @author Max Godefroy
 * @date 29/10/2026.
 */

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp>
#include <KryneEngine/Core/Graphics/Texture.hpp>
#include <KryneEngine/Core/Memory/GenerationalPool.hpp>
#include <KryneEngine/Core/Platform/StdAlloc.hpp>
#include <KryneEngine/Modules/RenderGraph/Descriptors/RenderTargetViewDesc.hpp>
#include <KryneEngine/Modules/RenderGraph/Registry.hpp>
#include <gtest/gtest.h>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Modules::RenderGraph::Tests
{
    using namespace KryneEngine::Tests;

    namespace
    {
        // Registered (non-owned) resources are never handed to the graphics context, so fabricated
        // handles and a null context are enough to exercise the registry's ref counting.
        TextureHandle FakeTextureHandle(const u32 _index) { return TextureHandle { GenPool::Handle { _index, 0 } }; }
        BufferHandle FakeBufferHandle(const u32 _index) { return BufferHandle { GenPool::Handle { _index, 0 } }; }
        TextureViewHandle FakeTextureViewHandle(const u32 _index) { return TextureViewHandle { GenPool::Handle { _index, 0 } }; }
        BufferViewHandle FakeBufferViewHandle(const u32 _index) { return BufferViewHandle { GenPool::Handle { _index, 0 } }; }
        RenderTargetViewHandle FakeRtvHandle(const u32 _index) { return RenderTargetViewHandle { GenPool::Handle { _index, 0 } }; }

        // Accessing a freed slot trips the pool's ref count assert, which is the only observable
        // difference between a live and a freed registry entry.
        bool IsAlive(const Registry& _registry, const SimplePoolHandle _resource)
        {
            ScopedAssertCatcher catcher;
            (void)_registry.GetResource(_resource);
            return catcher.GetCaughtMessages().empty();
        }

        class CountingAllocator final : public IAllocator
        {
        public:
            CountingAllocator() : IAllocator("RegistryTestAllocator") {}

            void* Allocate(const size_t _size, const size_t _alignment) override
            {
                m_liveAllocations++;
                return StdAlloc::MemAlign(_size, eastl::max(_alignment, alignof(max_align_t)));
            }

            void Free(void* _ptr, size_t) override
            {
                if (_ptr != nullptr)
                {
                    m_liveAllocations--;
                }
                StdAlloc::Free(_ptr);
            }

            s64 m_liveAllocations = 0;
        };
    }

    TEST(Registry, UnregisterRegisteredRawResources)
    {
        ScopedAssertCatcher catcher;
        Registry registry {{}};

        const SimplePoolHandle texture = registry.RegisterRawTexture(FakeTextureHandle(1));
        const SimplePoolHandle buffer = registry.RegisterRawBuffer(FakeBufferHandle(1));
        const SimplePoolHandle dummy = registry.RegisterDummy();

        EXPECT_EQ(registry.UnregisterResource(nullptr, texture), Registry::UnregisterResult::Freed);
        EXPECT_EQ(registry.UnregisterResource(nullptr, buffer), Registry::UnregisterResult::Freed);
        EXPECT_EQ(registry.UnregisterResource(nullptr, dummy), Registry::UnregisterResult::Freed);

        catcher.ExpectNoMessage();

        EXPECT_FALSE(IsAlive(registry, texture));
        EXPECT_FALSE(IsAlive(registry, buffer));
        EXPECT_FALSE(IsAlive(registry, dummy));
    }

    TEST(Registry, RegisteredTextureViewKeepsParentAlive)
    {
        ScopedAssertCatcher catcher;
        Registry registry {{}};

        const SimplePoolHandle texture = registry.RegisterRawTexture(FakeTextureHandle(1));
        const SimplePoolHandle view = registry.RegisterTextureView(FakeTextureViewHandle(1), texture);

        EXPECT_EQ(registry.UnregisterResource(nullptr, texture), Registry::UnregisterResult::Released);
        EXPECT_TRUE(IsAlive(registry, texture));

        EXPECT_EQ(registry.UnregisterResource(nullptr, view), Registry::UnregisterResult::Freed);

        catcher.ExpectNoMessage();

        EXPECT_FALSE(IsAlive(registry, view));
        EXPECT_FALSE(IsAlive(registry, texture));
    }

    TEST(Registry, UnregisterRegisteredViewBeforeParent)
    {
        ScopedAssertCatcher catcher;
        Registry registry {{}};

        const SimplePoolHandle texture = registry.RegisterRawTexture(FakeTextureHandle(1));
        const SimplePoolHandle rtv = registry.RegisterRenderTargetView(FakeRtvHandle(1), texture);

        EXPECT_EQ(registry.UnregisterResource(nullptr, rtv), Registry::UnregisterResult::Freed);
        EXPECT_FALSE(IsAlive(registry, rtv));
        EXPECT_TRUE(IsAlive(registry, texture));

        EXPECT_EQ(registry.UnregisterResource(nullptr, texture), Registry::UnregisterResult::Freed);

        catcher.ExpectNoMessage();

        EXPECT_FALSE(IsAlive(registry, texture));
    }

    TEST(Registry, RegisteredBufferViewKeepsParentAlive)
    {
        ScopedAssertCatcher catcher;
        Registry registry {{}};

        const SimplePoolHandle buffer = registry.RegisterRawBuffer(FakeBufferHandle(1));
        const SimplePoolHandle view = registry.RegisterBufferView(FakeBufferViewHandle(1), buffer);

        EXPECT_EQ(registry.UnregisterResource(nullptr, buffer), Registry::UnregisterResult::Released);
        EXPECT_TRUE(IsAlive(registry, buffer));

        EXPECT_EQ(registry.UnregisterResource(nullptr, view), Registry::UnregisterResult::Freed);

        catcher.ExpectNoMessage();

        EXPECT_FALSE(IsAlive(registry, view));
        EXPECT_FALSE(IsAlive(registry, buffer));
    }

    TEST(Registry, ParentOutlivesAllItsViews)
    {
        ScopedAssertCatcher catcher;
        Registry registry {{}};

        const SimplePoolHandle texture = registry.RegisterRawTexture(FakeTextureHandle(1));
        const SimplePoolHandle view = registry.RegisterTextureView(FakeTextureViewHandle(1), texture);
        const SimplePoolHandle rtv = registry.RegisterRenderTargetView(FakeRtvHandle(1), texture);

        EXPECT_EQ(registry.UnregisterResource(nullptr, texture), Registry::UnregisterResult::Released);
        EXPECT_EQ(registry.UnregisterResource(nullptr, view), Registry::UnregisterResult::Freed);
        EXPECT_TRUE(IsAlive(registry, texture));

        EXPECT_EQ(registry.UnregisterResource(nullptr, rtv), Registry::UnregisterResult::Freed);

        catcher.ExpectNoMessage();

        EXPECT_FALSE(IsAlive(registry, texture));
    }

    TEST(Registry, FreedSlotIsReused)
    {
        ScopedAssertCatcher catcher;
        Registry registry {{}};

        const SimplePoolHandle texture = registry.RegisterRawTexture(FakeTextureHandle(1));
        EXPECT_EQ(registry.UnregisterResource(nullptr, texture), Registry::UnregisterResult::Freed);

        const SimplePoolHandle buffer = registry.RegisterRawBuffer(FakeBufferHandle(1), "Reused");
        EXPECT_EQ(buffer, texture);
        EXPECT_EQ(registry.GetResource(buffer).m_type, ResourceType::Buffer);

        catcher.ExpectNoMessage();
    }

#if !defined(KE_FINAL)
    TEST(Registry, UnregisterReleasesResourceName)
    {
        ScopedAssertCatcher catcher;
        CountingAllocator allocator;

        {
            Registry registry { AllocatorInstance(&allocator) };
            const s64 baseline = allocator.m_liveAllocations;

            // Long enough to defeat the small string optimization, so the name owns a heap block.
            const SimplePoolHandle texture = registry.RegisterRawTexture(
                FakeTextureHandle(1),
                RawTextureData::kNoArrayPartialIndexing,
                RawTextureData::kNoMipPartialIndexing,
                "A texture name that is definitely longer than any small string buffer");
            EXPECT_GT(allocator.m_liveAllocations, baseline);

            EXPECT_EQ(registry.UnregisterResource(nullptr, texture), Registry::UnregisterResult::Freed);
            EXPECT_EQ(allocator.m_liveAllocations, baseline);
        }

        EXPECT_EQ(allocator.m_liveAllocations, 0);

        catcher.ExpectNoMessage();
    }
#endif

    // Needs a real graphics context: kept in a "Gpu" suite so runners without a usable graphics API
    // can skip it with `--gtest_filter=-*Gpu.*` (or the GTEST_FILTER environment variable).
    TEST(RegistryGpu, UnregisterOwnedResourcesDestroysThem)
    {
        ScopedAssertCatcher catcher;

        GraphicsCommon::ApplicationInfo appInfo {
            .m_applicationName = "Registry unit tests",
            .m_api =
#if defined(KE_GRAPHICS_API_DX12)
                GraphicsCommon::Api::DirectX12_1,
#elif defined(KE_GRAPHICS_API_VK)
                GraphicsCommon::Api::Vulkan_1_0,
#elif defined(KE_GRAPHICS_API_MTL)
                GraphicsCommon::Api::Metal_4,
#endif
            .m_features = {
                .m_present = false,
                .m_transferQueue = false,
            },
        };
        GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, AllocatorInstance());

        {
            Registry registry {{}};

            const SimplePoolHandle texture = registry.CreateRawTexture(graphicsContext, TextureCreateDesc {
                .m_desc = {
                    .m_dimensions = { 64, 64, 1 },
                    .m_format = TextureFormat::RGBA8_UNorm,
                    .m_arraySize = 1,
                    .m_type = TextureTypes::Single2D,
                    .m_mipCount = 1,
#if !defined(KE_FINAL)
                    .m_debugName = "Registry Test Texture",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType
                    | MemoryUsage::SampledImage
                    | MemoryUsage::ColorTargetImage,
            });

            const SimplePoolHandle view = registry.CreateTextureView(graphicsContext, texture, TextureViewDesc {
                .m_format = TextureFormat::RGBA8_UNorm,
            }, "Registry Test Texture View");

            const SimplePoolHandle rtv = registry.CreateRenderTargetView(graphicsContext, RenderTargetViewDesc {
                .m_textureResource = texture,
                .m_format = TextureFormat::RGBA8_UNorm,
            }, "Registry Test RTV");

            EXPECT_EQ(registry.UnregisterResource(graphicsContext, texture), Registry::UnregisterResult::Released);
            EXPECT_EQ(registry.UnregisterResource(graphicsContext, view), Registry::UnregisterResult::Freed);
            EXPECT_TRUE(IsAlive(registry, texture));

            EXPECT_EQ(registry.UnregisterResource(graphicsContext, rtv), Registry::UnregisterResult::Freed);
            EXPECT_FALSE(IsAlive(registry, texture));
        }

        // Any GPU resource left alive would be reported by the backend's leak checks on teardown.
        GraphicsContext::Destroy(graphicsContext);

        catcher.ExpectNoMessage();
    }
} // namespace KryneEngine::Modules::RenderGraph::Tests
