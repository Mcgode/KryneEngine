/**
 * @file
 * @author Max Godefroy
 * @date 12/09/2026.
 */

#pragma once

#include <EASTL/vector.h>
#include <KryneEngine/Core/Math/Quaternion.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Core/Memory/SimplePool.hpp>
#include <KryneEngine/Modules/EnTT/Context.hpp>
#include <box3d/box3d.h>

namespace KryneEngine
{
    class FibersManager;
}

namespace KryneEngine::Samples
{
    class DrawInstanceManager;
}

namespace KryneEngine::Samples::PhysicsDemo
{
    namespace Ecs = Modules::EnTT;

    struct Transform
    {
        float3 m_position {};
        Math::Quaternion m_rotation {};
        float3 m_scale { 1.f, 1.f, 1.f };
    };

    // Game registry components

    struct PhysicsBody
    {
        b3BodyId m_body {};
    };

    struct RenderModel
    {
        SimplePoolHandle m_model {};
    };

    /**
     * @brief Owns the world's entities, backed by two EnTT contexts: a game one, holding the authoritative state, and
     * a render one, mirroring it for interpolation.
     *
     * @details
     * Game registry entities are made of a `Transform`, a `PhysicsBody` and a `RenderModel`. Other components can be
     * added by the users (e.g. tags), as long as they follow the threading contract below.
     *
     * The render registry mirrors each game entity under the same identifier (through `Registry::create(hint)`), but
     * only holds what interpolation and rendering need.
     *
     * Threading contract:
     * - CreateEntity/DestroyEntity/Update, and any access to the game registry, must only happen from the fixed-step
     *   game-loop thread (SceneManager::GameLoop), or before the render loop starts. This is required by Box3D, which
     *   offers no guarantee that body creation/destruction is safe to call concurrently with b3World_Step.
     * - FlushEvents/SyncRenderInstances, and any access to the render registry, must only happen from the
     *   main/render thread.
     *
     * No data is ever shared/mutated across those two threads directly: all communication flows one way, in order,
     * through the internal event queue. The render registry is rebuilt purely by replaying the events it drains,
     * which is what lets SyncRenderInstances interpolate between the last two fixed steps without any locking.
     */
    class WorldObjectSystem
    {
    public:
        WorldObjectSystem(AllocatorInstance _allocator, b3WorldId _world, FibersManager* _fibersManager);
        ~WorldObjectSystem();

        // Creates the Box3D body (using _transform as its initial pose) and queues the registration of a render
        // instance of _renderModel. Call GetBody() right after to attach shapes (and typically
        // b3Body_ApplyMassFromShapes for dynamic bodies). The body's user data holds the entity, see GetEntity().
        [[nodiscard]] entt::entity CreateEntity(const Transform& _transform, b3BodyDef& _bodyDef, SimplePoolHandle _renderModel);

        [[nodiscard]] b3BodyId GetBody(entt::entity _entity) const;

        // Retrieves the entity owning a body created through CreateEntity(), or entt::null for any other body.
        [[nodiscard]] static entt::entity GetEntity(b3BodyId _body);

        void DestroyEntity(entt::entity _entity);

        void DestroyAllEntities();

        [[nodiscard]] Ecs::Registry& GetRegistry() { return m_gameContext.GetRegistry(); }

        // Must be called once per fixed physics step, right after b3World_Step.
        void Update();

        void FlushEvents(DrawInstanceManager& _drawInstanceManager);

        void SyncRenderInstances(DrawInstanceManager& _drawInstanceManager, float _alpha);

    private:
        enum GamePhase : u16
        {
            PostPhysics,
            GamePhaseCount,
        };

        enum RenderPhase : u16
        {
            Interpolation,
            RenderPhaseCount,
        };

        struct EntityEvent
        {
            enum class Kind : u8 { Created, Moved, Destroyed };

            Transform m_transform {};       // meaningful for Created/Moved
            SimplePoolHandle m_renderModel {}; // meaningful for Created only
            entt::entity m_entity = entt::null;
            Kind m_kind = Kind::Moved;
        };

        // Render registry components

        struct RenderTransforms
        {
            Transform m_previous {};
            Transform m_current {};
        };

        struct DrawInstance
        {
            SimplePoolHandle m_instance {};
        };

        // Game systems
        void ReadBodyTransforms(Ecs::View<const PhysicsBody, Transform> _view);
        void QueueMovedEvents(Ecs::View<const Transform, const RenderModel> _view);

        // Render systems
        void InterpolateRenderTransforms(Ecs::View<const RenderTransforms, Transform> _view);
        void PushDrawInstanceTransforms(Ecs::View<const Transform, const DrawInstance> _view);

        Ecs::Context m_gameContext;
        Ecs::Context m_renderContext;
        b3WorldId m_world;

        // Invariant contract : can only be written to by the game loop, and read by the render loop.
        // The render loop can only access it after ensuring game loop work is done, and before queueing some more.
        eastl::vector<EntityEvent> m_events;

        // Render systems parameters, only valid during SyncRenderInstances
        DrawInstanceManager* m_drawInstanceManager = nullptr;
        float m_interpolationAlpha = 0.f;
    };
}
