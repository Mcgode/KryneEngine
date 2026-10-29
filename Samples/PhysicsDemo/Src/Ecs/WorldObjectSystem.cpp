/**
 * @file
 * @author Max Godefroy
 * @date 12/09/2026.
 */

#include "WorldObjectSystem.hpp"

#include "Rendering/DrawInstanceManager.hpp"

#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>

namespace KryneEngine::Samples::PhysicsDemo
{
    namespace
    {
        b3Quat ToB3(const Math::Quaternion& _q)
        {
            return b3Quat {
                .v = {
                    .x = _q.x,
                    .y = _q.y,
                    .z = _q.z
                },
                .s = _q.w
            };
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

        // Offset by one, so entity 0 doesn't map to the null user data every other body has.
        void* EntityToUserData(const entt::entity _entity)
        {
            return reinterpret_cast<void*>(static_cast<uintptr_t>(entt::to_integral(_entity)) + 1);
        }

        entt::entity UserDataToEntity(void* _userData)
        {
            if (_userData == nullptr)
            {
                return entt::null;
            }
            return static_cast<entt::entity>(reinterpret_cast<uintptr_t>(_userData) - 1);
        }
    }

    WorldObjectSystem::WorldObjectSystem(
        const AllocatorInstance _allocator,
        const b3WorldId _world,
        FibersManager* _fibersManager)
            : m_gameContext(_allocator, GamePhaseCount, _fibersManager)
            , m_renderContext(_allocator, RenderPhaseCount, _fibersManager)
            , m_world(_world)
            , m_events(_allocator)
    {
        m_gameContext.AddSystem<&WorldObjectSystem::ReadBodyTransforms>(PostPhysics, *this, "ReadBodyTransforms");
        m_gameContext.AddSystem<&WorldObjectSystem::QueueMovedEvents>(PostPhysics, *this, "QueueMovedEvents");

        m_renderContext.AddSystem<&WorldObjectSystem::InterpolateRenderTransforms>(
            Interpolation, *this, "InterpolateRenderTransforms");
        m_renderContext.AddSystem<&WorldObjectSystem::PushDrawInstanceTransforms>(
            Interpolation, *this, "PushDrawInstanceTransforms");
    }

    WorldObjectSystem::~WorldObjectSystem() = default;

    entt::entity WorldObjectSystem::CreateEntity(
        const Transform& _transform,
        b3BodyDef& _bodyDef,
        const SimplePoolHandle _renderModel)
    {
        // float3 <-> b3Pos share the same {x, y, z} layout (same convention already used elsewhere,
        // e.g. PhysicsDemo.cpp's gravity vector); the quaternion layout differs and needs converting.
        _bodyDef.position = *reinterpret_cast<const b3Pos*>(&_transform.m_position);
        _bodyDef.rotation = ToB3(_transform.m_rotation);

        Ecs::Registry& registry = m_gameContext.GetRegistry();
        const entt::entity entity = registry.create();

        _bodyDef.userData = EntityToUserData(entity);

        registry.emplace<Transform>(entity, _transform);
        registry.emplace<PhysicsBody>(entity, b3CreateBody(m_world, &_bodyDef));
        registry.emplace<RenderModel>(entity, _renderModel);

        m_events.emplace_back(_transform, _renderModel, entity, EntityEvent::Kind::Created);

        return entity;
    }

    b3BodyId WorldObjectSystem::GetBody(const entt::entity _entity) const
    {
        const PhysicsBody* body = m_gameContext.GetRegistry().try_get<PhysicsBody>(_entity);
        return body != nullptr ? body->m_body : b3_nullBodyId;
    }

    entt::entity WorldObjectSystem::GetEntity(const b3BodyId _body)
    {
        return UserDataToEntity(b3Body_GetUserData(_body));
    }

    void WorldObjectSystem::DestroyEntity(const entt::entity _entity)
    {
        Ecs::Registry& registry = m_gameContext.GetRegistry();
        if (!registry.valid(_entity))
        {
            return;
        }

        if (const PhysicsBody* body = registry.try_get<PhysicsBody>(_entity))
        {
            b3DestroyBody(body->m_body);
        }
        registry.destroy(_entity);

        m_events.emplace_back(Transform {}, SimplePoolHandle {}, _entity, EntityEvent::Kind::Destroyed);
    }

    void WorldObjectSystem::DestroyAllEntities()
    {
        const auto view = m_gameContext.GetRegistry().view<const PhysicsBody>();
        eastl::vector<entt::entity> entities(m_events.get_allocator());
        entities.reserve(view.size());
        for (const entt::entity entity : view)
        {
            entities.push_back(entity);
        }

        for (const entt::entity entity : entities)
        {
            DestroyEntity(entity);
        }
    }

    void WorldObjectSystem::Update()
    {
        KE_ZoneScopedFunction("WorldObjectSystem::Update");

        m_gameContext.RunPhase(PostPhysics);
    }

    void WorldObjectSystem::FlushEvents(DrawInstanceManager& _drawInstanceManager)
    {
        KE_ZoneScopedFunction("WorldObjectSystem::FlushEvents");

        Ecs::Registry& registry = m_renderContext.GetRegistry();

        for (const auto& event: m_events)
        {
            switch (event.m_kind)
            {
            case EntityEvent::Kind::Created:
            {
                // Mirror the game entity, so later events map directly to it. A game entity identifier can only be
                // reused after its destruction, whose event was already replayed above.
                const entt::entity entity = registry.create(event.m_entity);
                KE_ASSERT(entity == event.m_entity);

                registry.emplace<RenderTransforms>(entity, event.m_transform, event.m_transform);
                registry.emplace<Transform>(entity, event.m_transform);
                registry.emplace<DrawInstance>(entity, _drawInstanceManager.RegisterInstance(
                    event.m_renderModel,
                    event.m_transform.m_position,
                    event.m_transform.m_rotation,
                    event.m_transform.m_scale));
                break;
            }
            case EntityEvent::Kind::Moved:
                if (RenderTransforms* transforms = registry.try_get<RenderTransforms>(event.m_entity))
                {
                    transforms->m_previous = transforms->m_current;
                    transforms->m_current = event.m_transform;
                }
                break;
            case EntityEvent::Kind::Destroyed:
                if (registry.valid(event.m_entity))
                {
                    if (const DrawInstance* drawInstance = registry.try_get<DrawInstance>(event.m_entity))
                    {
                        _drawInstanceManager.UnregisterInstance(drawInstance->m_instance);
                    }
                    registry.destroy(event.m_entity);
                }
                break;
            }
        }
        m_events.clear();
    }

    void WorldObjectSystem::SyncRenderInstances(DrawInstanceManager& _drawInstanceManager, const float _alpha)
    {
        KE_ZoneScopedFunction("WorldObjectSystem::SyncRenderInstances");

        m_drawInstanceManager = &_drawInstanceManager;
        m_interpolationAlpha = _alpha;
        m_renderContext.RunPhase(Interpolation);
        m_drawInstanceManager = nullptr;
    }

    void WorldObjectSystem::ReadBodyTransforms(Ecs::View<const PhysicsBody, Transform> _view)
    {
        for (auto [entity, body, transform]: _view.each())
        {
            const b3WorldTransform worldTransform = b3Body_GetTransform(body.m_body);
            transform.m_position = *reinterpret_cast<const float3*>(&worldTransform.p);
            transform.m_rotation = FromB3(worldTransform.q);
        }
    }

    void WorldObjectSystem::QueueMovedEvents(Ecs::View<const Transform, const RenderModel> _view)
    {
        for (auto [entity, transform, renderModel]: _view.each())
        {
            m_events.emplace_back(transform, SimplePoolHandle {}, entity, EntityEvent::Kind::Moved);
        }
    }

    void WorldObjectSystem::InterpolateRenderTransforms(Ecs::View<const RenderTransforms, Transform> _view)
    {
        for (auto [entity, transforms, transform]: _view.each())
        {
            transform = Interpolate(transforms.m_previous, transforms.m_current, m_interpolationAlpha);
        }
    }

    void WorldObjectSystem::PushDrawInstanceTransforms(Ecs::View<const Transform, const DrawInstance> _view)
    {
        KE_ASSERT(m_drawInstanceManager != nullptr);

        for (auto [entity, transform, drawInstance]: _view.each())
        {
            m_drawInstanceManager->SetInstanceTransform(
                drawInstance.m_instance,
                transform.m_position,
                transform.m_rotation,
                transform.m_scale);
        }
    }
} // namespace KryneEngine::Samples::PhysicsDemo
