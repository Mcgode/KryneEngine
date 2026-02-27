/**
 * @file
 * @author Max Godefroy
 * @date 06/07/2025.
 */

#include "KryneEngine/Modules/GuiLib/Context.hpp"

#include "KryneEngine/Modules/GuiLib/IGuiRenderer.hpp"

#include <KryneEngine/Core/Common/StringHelpers.hpp>
#include <KryneEngine/Core/Memory/Containers/StableVector.inl>
#include <KryneEngine/Modules/TextRendering/Font.hpp>
#include <KryneEngine/Modules/TextRendering/FontManager.hpp>

namespace KryneEngine::Modules::GuiLib
{
    Context::Context(const AllocatorInstance _allocator, TextRendering::FontManager* _fontManager)
        : m_allocator(_allocator)
        , m_fontManager(_fontManager)
        , m_registeredRegions(_allocator)
    {}

    void Context::Initialize(IGuiRenderer* _renderer, const uint2& _viewportSize)
    {
        const u32 arenaCapacity = Clay_MinMemorySize();
        m_arenaMemory = m_allocator.Allocate<char>(arenaCapacity);
        m_renderer = _renderer;

        const Clay_Arena arena {
            .capacity = arenaCapacity,
            .memory = m_arenaMemory
        };

        const Clay_Dimensions dimensions {
            .width = static_cast<float>(_viewportSize.x),
            .height = static_cast<float>(_viewportSize.y),
        };

        const Clay_ErrorHandler errorHandler {
            .errorHandlerFunction = ErrorHandler,
            .userData = this,
        };

        Clay_Initialize(arena, dimensions, errorHandler);
        m_clayContext = Clay_GetCurrentContext();

        Clay_SetMeasureTextFunction(MeasureText, this);

        Clay_SetCurrentContext(nullptr);
    }

    void Context::Destroy()
    {
        Clay_SetCurrentContext(nullptr);
        m_clayContext = nullptr;
        m_allocator.deallocate(m_arenaMemory);
    }

    void Context::BeginLayout(const uint2& _viewportSize, const float4x4& _projectionMatrix)
    {
        KE_ASSERT_MSG(Clay_GetCurrentContext() == nullptr, "Clay context is already set, either it was not reset properly, or there is a race condition.");
        Clay_SetCurrentContext(m_clayContext);
        Clay_SetLayoutDimensions({
            .width = static_cast<float>(_viewportSize.x),
            .height = static_cast<float>(_viewportSize.y),
        });
        m_renderer->BeginLayout(_projectionMatrix, _viewportSize);
    }

    void Context::EndLayout(
        GraphicsContext& _graphicsContext,
        CommandListHandle _transferCommandList,
        CommandListHandle _renderCommandList)
    {
        m_renderer->EndLayoutAndRender(_graphicsContext, _transferCommandList, _renderCommandList);
        Clay_SetCurrentContext(nullptr);
    }

    void* Context::RegisterTextureRegion(TextureRegion&& _region)
    {
        return &m_registeredRegions.PushBack(std::move(_region));
    }

    void Context::ErrorHandler(Clay_ErrorData _errorData)
    {
        KE_ERROR(_errorData.errorText.chars);
    }

    Clay_Dimensions Context::MeasureText(Clay_StringSlice _slice, Clay_TextElementConfig* _config, void* _userData)
    {
        if (_userData == nullptr)
            return Clay_Dimensions { .width = 0.f, .height = 0.f };

        const TextRendering::FontManager* fontManager = static_cast<Context*>(_userData)->m_fontManager;

        TextRendering::Font* font = fontManager->GetFont(_config->fontId);

        Clay_Dimensions dimensions {};
        float currentLineWidth = 0.f;

        const float ascender = font->GetAscender(_config->fontSize);
        const float descender = font->GetDescender(_config->fontSize);
        const float baseLineHeight = font->GetLineHeight(_config->fontSize);

        const eastl::string_view string { _slice.chars, static_cast<size_t>(_slice.length) };
        for (auto it = Utf8Iterator(string); it != string.end(); ++it)
        {
            const u32 unicodeCodepoint = *it;

            switch (unicodeCodepoint)
            {
                case '\n':
                    dimensions.height += baseLineHeight + static_cast<float>(_config->lineHeight);
                case '\r':
                    if (currentLineWidth > dimensions.width)
                        dimensions.width = currentLineWidth;
                    currentLineWidth = 0.f;
                    break;
                default:
                    if (currentLineWidth > 0.f)
                        currentLineWidth += static_cast<float>(_config->letterSpacing);
                    currentLineWidth += font->GetHorizontalAdvance(unicodeCodepoint, _config->fontSize);
                    break;
            }
        }
        // Don't add vertical spacing for the last line
        dimensions.height += ascender + abs(descender);
        if (currentLineWidth > dimensions.width)
            dimensions.width = currentLineWidth;

        return dimensions;
    }

    Context::~Context() = default;
} // namespace KryneEngine::Modules::Clay

