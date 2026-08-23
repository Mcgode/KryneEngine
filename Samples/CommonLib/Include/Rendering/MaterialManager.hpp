/**
 * @file
 * @author Max Godefroy
 * @date 20/08/2026.
 */

#pragma once


#include <EASTL/span.h>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Graphics/Handles.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>


namespace KryneEngine::Samples
{
    struct MaterialHandle
    {
        u32 m_index;
    };

    static constexpr MaterialHandle kInvalidMaterialHandle { UINT32_MAX };

    /**
     * @brief A manager for materials.
     *
     * @details
     * For each pass type, you can provide a corresponding graphics pipeline, and some custom descriptor sets.
     * The pipelines must include an implicit descriptor set, which will contain the pass-related descriptors. This set
     * is always the first descriptor set of the pipeline, for improved descriptor set swapping performance.
     * The descriptor set layout can be retrieved from `DrawInstanceManager::GetPassDescriptorSetLayout`.
     *
     * @note
     * This manager is meant to be used in conjunction with `DrawInstanceManager`.
     */
    class MaterialManager
    {
    public:
        struct MaterialPipeline
        {
            GraphicsPipelineHandle m_pipeline {};
            eastl::array<DescriptorSetHandle, 3> m_descriptorSets {};

            [[nodiscard]] bool operator<(const MaterialPipeline& _other) const;
        };

        MaterialManager(AllocatorInstance _allocator, u8 _passTypeCount, size_t _maxMaterialCount = 4'096);

        template<class Enum>
        explicit MaterialManager(const AllocatorInstance _allocator, const size_t _maxMaterialCount = 4'096)
            : MaterialManager(_allocator, static_cast<u8>(Enum::Count), _maxMaterialCount)
        {}

        ~MaterialManager();

        [[nodiscard]] u16 GetPassTypeCount() const { return m_passTypeCount; }

         MaterialHandle RegisterMaterial();

        void SetGraphicsPipeline(MaterialHandle _material, u8 _passType, GraphicsPipelineHandle _pipeline) const;
        [[nodiscard]] GraphicsPipelineHandle GetGraphicsPipeline(MaterialHandle _material, u8 _passType) const;

    private:
        AllocatorInstance m_allocator;

        eastl::vector<u32> m_materialsFreeList;
        u32 m_nextFreeMaterial;

        /**
         * @details
         * Material pipelines are stored in a flat array. One material can have multiple pipelines, up to one per pass type.
         *
         * The array is separated in `m_passTypeCount` sections, each section containing `m_maxMaterialCount` pipelines.
         * This layout was set up this way to account for usual cache access pattern: when multiple pipelines are
         * queried, they are most often queried for a specific pass type.
         */
        MaterialPipeline* m_pipelines = nullptr;

        u8 m_passTypeCount;
        size_t m_maxMaterialCount;
    };
}
