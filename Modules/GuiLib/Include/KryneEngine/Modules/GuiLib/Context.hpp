/**
 * @file
 * @author Max Godefroy
 * @date 06/07/2025.
 */

#pragma once

#include "KryneEngine/Core/Math/Matrix.hpp"


#include <KryneEngine/Core/Common/Utils/Macros.hpp>
#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Core/Memory/Containers/StableVector.hpp>
#include <clay.h>

#include "KryneEngine/Modules/GuiLib/TextureRegion.hpp"

namespace KryneEngine::Modules::TextRendering
{
    class FontManager;
}

namespace KryneEngine::Modules::GuiLib
{
    class IGuiRenderer;

    class Context
    {
    public:
        Context(AllocatorInstance _allocator, TextRendering::FontManager* _fontManager);

        KE_DEFINE_COPY_MOVE_SEMANTICS(Context, delete, delete);

        void Initialize(IGuiRenderer* _renderer, const uint2& _viewportSize);

        void Destroy();

        void BeginLayout(const uint2& _viewportSize, const float4x4& _projectionMatrix = float4x4());
        void EndLayout(
            GraphicsContext& _graphicsContext,
            CommandListHandle _transferCommandList,
            CommandListHandle _renderCommandList);

        void* RegisterTextureRegion(TextureRegion&& _region);

        ~Context();

        [[nodiscard]] AllocatorInstance GetAllocator() const { return m_allocator; }

    private:
        AllocatorInstance m_allocator;
        TextRendering::FontManager* m_fontManager;
        char* m_arenaMemory = nullptr;
        Clay_Context* m_clayContext = nullptr;
        IGuiRenderer* m_renderer = nullptr;

        /// A temporary array for storing texture regions for this Gui context
        StableVector<TextureRegion> m_registeredRegions;

        static void ErrorHandler(Clay_ErrorData _errorData);

        static Clay_Dimensions MeasureText(Clay_StringSlice _slice, Clay_TextElementConfig* _config, void* _userData);
    };
} // namespace KryneEngine::Modules::Clay

