/**
 * @file
 * @author Max Godefroy
 * @date 23/08/2026.
 */

#pragma once

#include <EASTL/optional.h>
#include <EASTL/vector_set.h>
#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Graphics/Handles.hpp>
#include <KryneEngine/Core/Math/Matrix.hpp>
#include <KryneEngine/Core/Memory/SimplePool.hpp>
#include <KryneEngine/Modules/GraphicsUtils/DynamicBuffer.hpp>


namespace KryneEngine::Samples
{
    class MaterialManager;
    class DrawInstanceManager;


    class PassDispatcher
    {
        friend DrawInstanceManager;

    public:
        struct PassConstantBuffer
        {
            float4x4 m_viewProjectionMatrix;
            float4x4 m_viewMatrix;
            float4x4 m_projectionMatrix;
        };

        ~PassDispatcher();

        void PrepareDispatch(
            const float4x4& _viewMatrix,
            const float4x4& _projectionMatrix,
            GraphicsContext& _graphicsContext,
            TransferCommandEncoderHandle _transferEncoder);

        void Dispatch(GraphicsContext& _graphicsContext, RenderCommandEncoderHandle _renderEncoder);

    protected:
        PassDispatcher(
            DrawInstanceManager* _drawInstanceManager,
            const MaterialManager* _materialManager,
            GraphicsContext& _graphicsContext,
            u8 _passType,
            eastl::string_view _debugName);

    private:
        DrawInstanceManager* m_drawInstanceManager;
        const MaterialManager* m_materialManager;
        DescriptorSetHandle m_passDescriptorSet;
        u8 m_passType;

        Modules::GraphicsUtils::DynamicBuffer m_instanceBuffer;
        Modules::GraphicsUtils::DynamicBuffer m_constantBuffer;

        BufferViewHandle* m_constantBufferViews = nullptr;

        struct DispatchData
        {
            eastl::vector_set<SimplePoolHandle> m_models;
            eastl::vector<u32> m_modelInstanceOffsets;
        };
        eastl::optional<DispatchData> m_dispatchData;
    };
}