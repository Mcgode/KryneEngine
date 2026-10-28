/**
 * @file
 * @author Max Godefroy
 * @date 28/10/2026.
 */

#pragma once

#include <entt/entity/registry.hpp>
#include <KryneEngine/Core/Memory/Allocators/StdAllocator.hpp>

namespace KryneEngine::Modules::EnTT
{
    using Registry = entt::basic_registry<entt::entity, StdAllocator<entt::entity>>;

    class Context
    {
    public:
        explicit Context(AllocatorInstance _allocator);

        [[nodiscard]] Registry& GetRegistry() { return m_registry; }
        [[nodiscard]] const Registry& GetRegistry() const { return m_registry; }

    private:
        Registry m_registry;
    };
}
