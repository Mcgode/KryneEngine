/**
 * @file
 * @author Max Godefroy
 * @date 20/08/2026.
 */

#pragma once


#include <KryneEngine/Core/Graphics/Handles.hpp>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>


namespace KryneEngine::Samples
{
    struct ModelHandle
    {
        u32 m_index;
    };

    static constexpr ModelHandle kInvalidModelHandle { UINT32_MAX };

    class ModelManager
    {
    public:
        ModelManager(AllocatorInstance _allocator, u8 _passTypeCount, size_t _maxModelCount = 4'096);

        template<class Enum>
        explicit ModelManager(const AllocatorInstance _allocator, const size_t _maxModelCount = 4'096)
            : ModelManager(_allocator, static_cast<u8>(Enum::Count), _maxModelCount)
        {}

        ~ModelManager();

        [[nodiscard]] u16 GetPassTypeCount() const { return m_passTypeCount; }

         ModelHandle RegisterModel();

        void SetGraphicsPipeline(ModelHandle _model, u8 _passType, GraphicsPipelineHandle _pipeline) const;
        [[nodiscard]] GraphicsPipelineHandle GetGraphicsPipeline(ModelHandle _model, u8 _passType) const;

    private:
        AllocatorInstance m_allocator;

        eastl::vector<u32> m_modelsFreeList;
        u32 m_nextFreeModel;

        /**
         * @details
         * Pipelines are stored in a flat array. One model can have multiple pipelines, up to one per pass type.
         *
         * The array is separated in `m_passTypeCount` sections, each section containing `m_maxModelCount` pipelines.
         * This layout was set up this way to account for usual cache access pattern: when multiple pipelines are
         * queried, they are most often queried for a specific pass type.
         */
        GraphicsPipelineHandle* m_pipelines = nullptr;

        u8 m_passTypeCount;
        size_t m_maxModelCount;
    };
}
