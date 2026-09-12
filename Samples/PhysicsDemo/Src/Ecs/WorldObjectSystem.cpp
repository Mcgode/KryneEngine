/**
 * @file
 * @author Max Godefroy
 * @date 12/09/2026.
 */

#include "WorldObjectSystem.hpp"

#include "Rendering/DrawInstanceManager.hpp"

#include <EASTL/algorithm.h>
#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Memory/GenerationalPool.inl>

namespace KryneEngine::Samples::PhysicsDemo
{
    namespace
    {
        b3Quat ToB3(const Math::Quaternion& _q)
        {
            return b3Quat { { _q.x, _q.y, _q.z }, _q.w };
        }

        Math::Quaternion FromB3(const b3Quat& _q)
        {
            return Math::Quaternion(_q.s, _q.v.x, _q.v.y, _q.v.z);
        }

        Transform Interpolate(const Transform& _previous, const Transform& _current, const float _alpha)
        {
            Math::Quaternion rotation = _previous.m_rotation;
            rotation.Slerp(_current.m_rotation, _alpha);
            return Transform {
                .m_position = _previous.m_position + (_current.m_position - _previous.m_position) * _alpha,
                .m_rotation = rotation,
                .m_scale = _current.m_scale,
            };
        }
    }

    WorldObjectSystem::WorldObjectSystem(
        const AllocatorInstance _allocator,
        const b3WorldId _world,
        const size_t _maxPendingEvents)
            : m_allocator(_allocator)
            , m_world(_world)
            , m_entities(_allocator)
            , m_liveHandles(_allocator)
            , m_events(_allocator, _maxPendingEvents)
            , m_renderStates(_allocator)
    {}

    WorldObjectSystem::~WorldObjectSystem() = default;

    EntityHandle WorldObjectSystem::CreateEntity(
        const Transform& _transform,
        b3BodyDef _bodyDef,
        const SimplePoolHandle _renderModel)
    {
        // float3 <-> b3Pos share the same {x, y, z} layout (same convention already used elsewhere,
        // e.g. PhysicsDemo.cpp's gravity vector); the quaternion layout differs and needs converting.
        _bodyDef.position = *reinterpret_cast<const b3Pos*>(&_transform.m_position);
        _bodyDef.rotation = ToB3(_transform.m_rotation);

        const EntityHandle handle { m_entities.Allocate() };
        EntityInternal* entity = m_entities.Get(handle.m_handle);
        KE_ASSERT(entity != nullptr);

        entity->m_body = b3CreateBody(m_world, &_bodyDef);
        entity->m_transform = _transform;

        m_liveHandles.push_back(handle);

        KE_VERIFY(m_events.TryEmplace(_transform, _renderModel, handle, EntityEvent::Kind::Created));

        return handle;
    }

    b3BodyId WorldObjectSystem::GetBody(const EntityHandle _entity) const
    {
        const EntityInternal* entity = m_entities.Get(_entity.m_handle);
        return entity != nullptr ? entity->m_body : b3_nullBodyId;
    }

    void WorldObjectSystem::DestroyEntity(const EntityHandle _entity)
    {
        EntityInternal copy {};
        if (!m_entities.Free(_entity.m_handle, &copy))
        {
            return;
        }

        b3DestroyBody(copy.m_body);

        const auto it = eastl::find(m_liveHandles.begin(), m_liveHandles.end(), _entity);
        if (it != m_liveHandles.end())
        {
            m_liveHandles.erase_unsorted(it);
        }

        KE_VERIFY(m_events.TryEmplace(Transform {}, SimplePoolHandle {}, _entity, EntityEvent::Kind::Destroyed));
    }

    void WorldObjectSystem::Update()
    {
        for (const EntityHandle handle : m_liveHandles)
        {
            EntityInternal* entity = m_entities.Get(handle.m_handle);
            KE_ASSERT(entity != nullptr);

            const b3WorldTransform worldTransform = b3Body_GetTransform(entity->m_body);
            entity->m_transform.m_position = *reinterpret_cast<const float3*>(&worldTransform.p);
            entity->m_transform.m_rotation = FromB3(worldTransform.q);

            KE_VERIFY(m_events.TryEmplace(entity->m_transform, SimplePoolHandle {}, handle, EntityEvent::Kind::Moved));
        }
    }

    void WorldObjectSystem::SyncRenderInstances(DrawInstanceManager& _drawInstanceManager, const float _alpha)
    {
        for (EntityEvent* event; (event = m_events.Front()) != nullptr; m_events.Pop())
        {
            const size_t index = event->m_entity.m_handle.m_index;
            if (index >= m_renderStates.size())
            {
                m_renderStates.resize(index + 1);
            }
            RenderEntityState& state = m_renderStates[index];

            switch (event->m_kind)
            {
            case EntityEvent::Kind::Created:
                state.m_previous = state.m_current = event->m_transform;
                state.m_drawInstance = _drawInstanceManager.RegisterInstance(
                    event->m_renderModel,
                    event->m_transform.m_position,
                    event->m_transform.m_rotation,
                    event->m_transform.m_scale);
                state.m_registered = true;
                break;
            case EntityEvent::Kind::Moved:
                state.m_previous = state.m_current;
                state.m_current = event->m_transform;
                break;
            case EntityEvent::Kind::Destroyed:
                if (state.m_registered)
                {
                    _drawInstanceManager.UnregisterInstance(state.m_drawInstance);
                    state.m_registered = false;
                }
                break;
            }
        }

        // Only now that this frame's "destroyed" events have all been drained (and their render
        // instances unregistered) is it safe to let CreateEntity reuse those slots.
        m_entities.FlushDeferredFrees();

        for (RenderEntityState& state : m_renderStates)
        {
            if (!state.m_registered)
            {
                continue;
            }

            const Transform interpolated = Interpolate(state.m_previous, state.m_current, _alpha);
            _drawInstanceManager.SetInstanceTransform(
                state.m_drawInstance,
                interpolated.m_position,
                interpolated.m_rotation,
                interpolated.m_scale);
        }
    }
} // namespace KryneEngine::Samples::PhysicsDemo
