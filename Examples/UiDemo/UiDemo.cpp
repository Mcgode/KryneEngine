/**
 * @file
 * @author Max Godefroy
 * @date 24/11/2025.
 */

#include "TextureGenerator.hpp"
#include "UiCube.hpp"
#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Graphics/RenderPass.hpp>
#include <KryneEngine/Core/Memory/Allocators/TlsfAllocator.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
#include <KryneEngine/Modules/GuiLib/Context.hpp>
#include <KryneEngine/Modules/GuiLib/GuiRenderers/BasicGuiRenderer.hpp>
#include <KryneEngine/Modules/Resources/Loaders/SerialResourceLoader.hpp>
#include <KryneEngine/Modules/Resources/RuntimeResourceSystem.hpp>
#include <KryneEngine/Modules/TextRendering/Font.hpp>
#include <KryneEngine/Modules/TextRendering/FontManager.hpp>
#include <KryneEngine/Modules/TextRendering/MsdfAtlasManager.hpp>

using KryneEngine::s32;


const Clay_Color COLOR_LIGHT = (Clay_Color) {224, 215, 210, 255};
const Clay_Color COLOR_RED = (Clay_Color) {168, 66, 28, 255};
const Clay_Color COLOR_ORANGE = (Clay_Color) {225, 138, 50, 255};

// Layout config is just a struct that can be declared statically, or inline
Clay_ElementDeclaration sidebarItemConfig = (Clay_ElementDeclaration) {
    .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(50) }
    },
    .backgroundColor = COLOR_ORANGE
};

// Re-useable components are just normal functions
void SidebarItemComponent() {
    CLAY(sidebarItemConfig) {
        // children go here...
    }
}

s32 main(s32 argc, const char** argv)
{
    auto* allocator = TlsfAllocator::Create({}, 16 << 20);
    AllocatorInstance allocatorInstance {allocator};

    auto appInfo = GraphicsCommon::ApplicationInfo {
        .m_applicationName { "UiDemo - Kryne Engine 2", allocator }
    };
#if defined(KE_GRAPHICS_API_VK)
    appInfo.m_api = GraphicsCommon::Api::Vulkan_1_3;
    appInfo.m_applicationName += " - Vulkan";
#elif defined(KE_GRAPHICS_API_DX12)
    appInfo.m_api = KryneEngine::GraphicsCommon::Api::DirectX12_1;
    appInfo.m_applicationName += " - DirectX 12";
#elif defined(KE_GRAPHICS_API_MTL)
    appInfo.m_api = KryneEngine::GraphicsCommon::Api::Metal_3;
    appInfo.m_applicationName += " - Metal";
#endif
    Window mainWindow(appInfo, allocator);
    GraphicsContext* graphicsContext = mainWindow.GetGraphicsContext();

    TextureGenerator textureGenerator { allocatorInstance, 33 };
    SamplerHandle sampler = graphicsContext->CreateSampler({
        .m_minFilter = SamplerDesc::Filter::Point,
        .m_magFilter = SamplerDesc::Filter::Point,
    });

    DynamicArray<RenderPassHandle> renderPassHandles(allocator);
    renderPassHandles.Resize(graphicsContext->GetFrameContextCount());
    for (auto i = 0u; i < renderPassHandles.Size(); i++)
    {
        renderPassHandles[i] = graphicsContext->CreateRenderPass({
            .m_colorAttachments = {
                RenderPassDesc::Attachment {
                    .m_loadOperation = RenderPassDesc::Attachment::LoadOperation::Clear,
                    .m_storeOperation = RenderPassDesc::Attachment::StoreOperation::Store,
                    .m_finalLayout = TextureLayout::Present,
                    .m_rtv = graphicsContext->GetPresentRenderTargetView(i),
                }
            },
#if !defined(KE_FINAL)
            .m_debugName = "Main render pass",
#endif
        });
    }

    Modules::Resources::SerialResourceLoader resourceLoader { allocatorInstance };
    Modules::Resources::RuntimeResourceSystem resourceSystem { allocatorInstance, &resourceLoader };

    Modules::TextRendering::FontManager fontManager(allocatorInstance);
    resourceSystem.RegisterResourceManager<Modules::TextRendering::Font>(&fontManager);

    Modules::TextRendering::MsdfAtlasManager msdfAtlasManager(allocatorInstance, graphicsContext, &fontManager, 1024, 32);

    const StringHash notoFontPath { "Resources/Modules/TextRendering/NotoSerif-Regular.ttf" };
    Modules::Resources::ResourceEntry* notFontEntry = resourceSystem.GetResourceEntry<Modules::TextRendering::Font>(notoFontPath);
    resourceSystem.LoadResource(notoFontPath, notFontEntry);
    auto* font = notFontEntry->UseResource<Modules::TextRendering::Font>();

    Modules::GuiLib::Context clayContext { allocatorInstance, &fontManager };
    Modules::GuiLib::BasicGuiRenderer guiRenderer {
        allocatorInstance,
        graphicsContext,
        renderPassHandles[0],
        sampler
    };
    guiRenderer.SetAtlasManager(&msdfAtlasManager);
    clayContext.Initialize(
        &guiRenderer,
        graphicsContext->GetPresentFrameBufferSize());

    UiCube uiCube { allocatorInstance, *graphicsContext, &fontManager, renderPassHandles[0], &msdfAtlasManager };

    do
    {
        KE_ZoneScoped("Render loop");

        const float2 dpiScale = mainWindow.GetDpiScale();
        const float contentScale = (dpiScale.x + dpiScale.y) / 2.f;

        CommandListHandle transferCommandList = graphicsContext->BeginGraphicsCommandList();
        CommandListHandle renderCommandList = graphicsContext->BeginGraphicsCommandList();

        {
            KE_ZoneScoped("Texture upload");
            textureGenerator.HandleUpload(*graphicsContext, transferCommandList);
        }

        clayContext.BeginLayout(graphicsContext->GetPresentFrameBufferSize());

        // An example of laying out a UI with a fixed width sidebar and flexible width main content
        CLAY({
            .id = CLAY_ID("OuterContainer"),
            .layout = {
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                .padding = CLAY_PADDING_ALL(static_cast<u16>(16.f * contentScale)),
                .childGap = static_cast<u16>(16.f * contentScale) },
            .backgroundColor = {250,250,255,255},
            .cornerRadius = { 10 * contentScale, 20 * contentScale, 40 * contentScale, 0 } })
        {
            CLAY({
                .id = CLAY_ID("SideBar"),
                .layout = {
                    .sizing = { .width = CLAY_SIZING_FIXED(300 * contentScale), .height = CLAY_SIZING_GROW(0) },
                    .padding = CLAY_PADDING_ALL(static_cast<u16>(16.f * contentScale)),
                    .childGap = static_cast<u16>(16.f * contentScale),
                    .layoutDirection = CLAY_TOP_TO_BOTTOM
                },
                .backgroundColor = COLOR_LIGHT,
                .cornerRadius = {
                    .topLeft = 10 * contentScale,
                    .topRight = 20 * contentScale,
                    .bottomLeft = 40 * contentScale,
                    .bottomRight = 0
                }
            })
            {
                CLAY({
                    .id = CLAY_ID("ProfilePictureOuter"),
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_GROW(0) },
                        .padding = CLAY_PADDING_ALL(static_cast<u16>(16.f * contentScale)),
                        .childGap = static_cast<u16>(16.f * contentScale),
                        .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
                    },
                    .backgroundColor = COLOR_RED })
                {
                    CLAY({
                        .id = CLAY_ID("ProfilePicture"),
                        .layout = { .sizing = { .width = CLAY_SIZING_FIXED(64 * contentScale), .height = CLAY_SIZING_FIXED(64 * contentScale) }},
                        .image = { .imageData = clayContext.RegisterTextureRegion({
                            .m_textureView = textureGenerator.GetTextureView(32),
                        })}
                    })
                    {}
                    CLAY_TEXT(
                        CLAY_STRING("Clay - UI Library"),
                        CLAY_TEXT_CONFIG({
                            .textColor = {255, 255, 255, 255},
                            .fontId = font->GetId(),
                            .fontSize = static_cast<u16>(20.f * contentScale)}));
                }

                // Standard C code like loops etc work inside components
                for (int i = 0; i < 5; i++) {
                    SidebarItemComponent();
                }
            }

            CLAY({
                .id = CLAY_ID("MainContent"),
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                    .padding = CLAY_PADDING_ALL(static_cast<u16>(16.f * contentScale)),
                    .childGap = static_cast<u16>(16.f * contentScale),
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .backgroundColor = COLOR_LIGHT,
                .cornerRadius = {
                    .topLeft = 10 * contentScale,
                    .topRight = 20 * contentScale,
                    .bottomLeft = 40 * contentScale,
                    .bottomRight = 0
                },
                .border = {
                    .color = Clay_Color { 10, 0, 0, 255 },
                    .width = {
                        .left = static_cast<u16>(1 * contentScale),
                        .right = static_cast<u16>(1.f * contentScale),
                        .top = static_cast<u16>(1.f * contentScale),
                        .bottom = static_cast<u16>(10.f * contentScale),
                    }
                },
            })
            {
                CLAY({ .layout = { .sizing = { .height = CLAY_SIZING_GROW() } } })
                {
                    CLAY_TEXT(CLAY_STRING("Un peu de français à afficher, bébé!"), CLAY_TEXT_CONFIG({
                        .textColor = { 255, 80, 80, 255 },
                        .fontId = font->GetId(),
                        .fontSize = static_cast<u16>(50.f * contentScale),
                        .letterSpacing = static_cast<u16>(2.f * contentScale),
                        .textAlignment = CLAY_TEXT_ALIGN_CENTER,
                    }));
                }

                for (auto i = 0; i < 4; i++)
                {
                    CLAY({ .layout = { .sizing = { .width = CLAY_SIZING_GROW() }, .childGap = static_cast<u16>(16.f * contentScale), .layoutDirection = CLAY_LEFT_TO_RIGHT } })
                    {
                        CLAY({ .layout = { .sizing = { .width = CLAY_SIZING_GROW() } } }) {}
                        for (auto j = 0; j < 8; j++)
                        {
                            Clay_CornerRadius cornerRadius = {};

                            if (i == 0)
                            {
                                if (j == 0)
                                    cornerRadius.topLeft = 10;
                                else if (j == 7)
                                    cornerRadius.topRight = 10;
                            }
                            else if (i == 3)
                            {
                                if (j == 0)
                                    cornerRadius.bottomLeft = 10;
                                else if (j == 7)
                                    cornerRadius.bottomRight = 10;
                            }

                            CLAY({
                                .layout = { .sizing = {
                                    .width = CLAY_SIZING_FIXED(64.f * contentScale),
                                    .height = CLAY_SIZING_FIXED(64.f * contentScale) } },
                                .backgroundColor = Clay_Color( 180, 180, 180, 255 ),
                                .cornerRadius = cornerRadius,
                                .image = {
                                    .imageData = clayContext.RegisterTextureRegion({
                                        .m_textureView = textureGenerator.GetTextureView(i * 8 + j),
                                    }),
                                },
                            }) {}
                        }
                        CLAY({ .layout = { .sizing = { .width = CLAY_SIZING_GROW() } } }) {}
                    }
                }
                CLAY({ .layout = { .sizing = { .height = CLAY_SIZING_GROW() } } })
                {
                    CLAY_TEXT(CLAY_STRING("日本語のグリフも表示できます!"), CLAY_TEXT_CONFIG({
                        .textColor = { 255, 80, 80, 255 },
                        .fontId = font->GetId(),
                        .fontSize = static_cast<u16>(60.f * contentScale),
                        .letterSpacing = static_cast<u16>(2.f * contentScale),
                        .textAlignment = CLAY_TEXT_ALIGN_CENTER,
                    }));
                }
            }
        }

        const RenderPassHandle currentPass = renderPassHandles[graphicsContext->GetCurrentPresentImageIndex()];
        graphicsContext->BeginRenderPass(renderCommandList, currentPass);
        clayContext.EndLayout(*graphicsContext, transferCommandList, renderCommandList);

        uiCube.Render(*graphicsContext, transferCommandList, renderCommandList, contentScale);
        graphicsContext->EndRenderPass(renderCommandList);

        msdfAtlasManager.FlushLoads(*graphicsContext, transferCommandList);

        graphicsContext->EndGraphicsCommandList(transferCommandList);
        graphicsContext->EndGraphicsCommandList(renderCommandList);
    }
    while (graphicsContext->EndFrame());

    clayContext.Destroy();
    GraphicsContext::Destroy(graphicsContext);

    return 0;
}