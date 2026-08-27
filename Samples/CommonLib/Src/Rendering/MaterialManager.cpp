/**
 * @file
 * @author Max Godefroy
 * @date 20/08/2026.
 */

#include "Rendering/MaterialManager.hpp"

namespace KryneEngine::Samples
{
    bool MaterialManager::MaterialPipeline::operator<(const MaterialPipeline& _other) const
    {
        if (m_pipelineLayout == _other.m_pipelineLayout)
        {
            if (m_pipeline == _other.m_pipeline)
            {
                for (size_t i = 0; i < m_descriptorSets.max_size(); i++)
                {
                    if (m_descriptorSets[i] == GenPool::kInvalidHandle)
                    {
                        return true;
                    }
                    else if (m_descriptorSets[i] != _other.m_descriptorSets[i])
                    {
                        return m_descriptorSets[i] < _other.m_descriptorSets[i];
                    }
                }
                return false;
            }
            return m_pipeline < _other.m_pipeline;
        }
        return m_pipelineLayout < _other.m_pipelineLayout;
    }

    MaterialManager::MaterialManager(const AllocatorInstance _allocator, const u8 _passTypeCount, const size_t _maxMaterialCount)
        : m_allocator(_allocator)
        , m_materialsFreeList(_allocator)
        , m_nextFreeMaterial(0)
        , m_passTypeCount(_passTypeCount)
        , m_maxMaterialCount(_maxMaterialCount)
    {
        m_pipelines = m_allocator.Allocate<MaterialPipeline>(m_maxMaterialCount * m_passTypeCount);
    }

    MaterialManager::~MaterialManager()
    {
        if (m_pipelines)
            m_allocator.deallocate(m_pipelines, sizeof(MaterialPipeline) * m_maxMaterialCount * m_passTypeCount);
    }

    MaterialHandle MaterialManager::RegisterMaterial()
    {
        if (m_materialsFreeList.empty())
        {
            if (m_nextFreeMaterial < m_maxMaterialCount)
                return { m_nextFreeMaterial++ };
            return kInvalidMaterialHandle;
        }

        const u32 value = m_materialsFreeList.back();
        m_materialsFreeList.pop_back();
        return { value };
    }

    void MaterialManager::SetPipelineLayout(
        const MaterialHandle _material,
        const u8 _passType,
        const PipelineLayoutHandle _pipelineLayout) const
    {
        const size_t index = _passType * m_maxMaterialCount + _material.m_index;
        m_pipelines[index].m_pipelineLayout = _pipelineLayout;
    }

    void MaterialManager::SetGraphicsPipeline(
        const MaterialHandle _material,
        const u8 _passType,
        const GraphicsPipelineHandle _pipeline) const
    {
        const size_t index = _passType * m_maxMaterialCount + _material.m_index;
        m_pipelines[index].m_pipeline = _pipeline;
    }

    GraphicsPipelineHandle MaterialManager::GetGraphicsPipeline(const MaterialHandle _material, const u8 _passType) const
    {
        const size_t index = _passType * m_maxMaterialCount + _material.m_index;
        return m_pipelines[index].m_pipeline;
    }

    const MaterialManager::MaterialPipeline* MaterialManager::GetMaterialPipeline(
        const MaterialHandle _material,
        const u8 _passType) const
    {
        const size_t index = _passType * m_maxMaterialCount + _material.m_index;
        return m_pipelines + index;
    }
} // namespace KryneEngine::Samples