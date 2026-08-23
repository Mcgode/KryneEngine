/**
 * @file
 * @author Max Godefroy
 * @date 20/08/2026.
 */

#include "Rendering/ModelManager.hpp"

namespace KryneEngine::Samples
{
    bool ModelManager::ModelPipeline::operator<(const ModelPipeline& _other) const
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

    ModelManager::ModelManager(const AllocatorInstance _allocator, const u8 _passTypeCount, const size_t _maxModelCount)
        : m_allocator(_allocator)
        , m_modelsFreeList(_allocator)
        , m_nextFreeModel(0)
        , m_passTypeCount(_passTypeCount)
        , m_maxModelCount(_maxModelCount)
    {
        m_pipelines = m_allocator.Allocate<ModelPipeline>(m_maxModelCount * m_passTypeCount);
    }

    ModelManager::~ModelManager()
    {
        if (m_pipelines)
            m_allocator.deallocate(m_pipelines, sizeof(ModelPipeline) * m_maxModelCount * m_passTypeCount);
    }

    ModelHandle ModelManager::RegisterModel()
    {
        if (m_modelsFreeList.empty())
        {
            if (m_nextFreeModel < m_maxModelCount)
                return { m_nextFreeModel++ };
            return kInvalidModelHandle;
        }

        const u32 value = m_modelsFreeList.back();
        m_modelsFreeList.pop_back();
        return { value };
    }

    void ModelManager::SetGraphicsPipeline(
        const ModelHandle _model,
        const u8 _passType,
        const GraphicsPipelineHandle _pipeline) const
    {
        const size_t index = _passType * m_maxModelCount + _model.m_index;
        m_pipelines[index].m_pipeline = _pipeline;
    }

    GraphicsPipelineHandle ModelManager::GetGraphicsPipeline(const ModelHandle _model, const u8 _passType) const
    {
        const size_t index = _passType * m_maxModelCount + _model.m_index;
        return m_pipelines[index].m_pipeline;
    }
} // namespace KryneEngine::Samples