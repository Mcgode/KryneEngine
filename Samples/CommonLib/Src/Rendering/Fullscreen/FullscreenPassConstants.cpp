/**
 * @file
 * @author Max Godefroy
 * @date 29/08/2026.
 */

#include "Rendering/Fullscreen/FullscreenPassConstants.hpp"

#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"

namespace KryneEngine::Samples::FullscreenPassCommon
{
    DescriptorSetLayoutHandle CreateConstantsDescriptorSetLayout(
        GraphicsContext* _graphicsContext,
        u32* _bindingIndices)
    {
        const DescriptorBindingDesc bindings[] {
            {
                .m_type = DescriptorBindingDesc::Type::ConstantBuffer,
                .m_visibility = ShaderVisibility::All,
            },
        };
        const DescriptorSetDesc desc { .m_bindings = bindings };

        u32 localIndex;
        return _graphicsContext->CreateDescriptorSetLayout(
            desc,
            _bindingIndices != nullptr ? _bindingIndices : &localIndex);
    }
}
