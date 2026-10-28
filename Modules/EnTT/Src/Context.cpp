/**
 * @file
 * @author Max Godefroy
 * @date 28/10/2026.
 */

#include "KryneEngine/Modules/EnTT/Context.hpp"

namespace KryneEngine::Modules::EnTT
{
    Context::Context(const AllocatorInstance _allocator)
        : m_registry(StdAllocator<entt::entity>(_allocator))
    {}
}
