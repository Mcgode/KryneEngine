/**
 * @file
 * @author Max Godefroy
 * @date 30/11/2025.
 */

#pragma once
#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Math/Matrix.hpp"

namespace KryneEngine
{
    class GraphicsContext;
}

namespace KryneEngine::Modules::GuiLib
{
    class IGuiRenderer
    {
    public:
        virtual ~IGuiRenderer() = default;

        virtual void BeginLayout(const float4x4& _viewportTransform, const uint2& _viewportSize) = 0;

        virtual void EndLayoutAndRender(
            GraphicsContext& _graphicsContext,
            CommandListHandle _transferCommandList,
            RenderCommandEncoderHandle _renderEncoder) = 0;
    };
} // namespace KryneEngine
