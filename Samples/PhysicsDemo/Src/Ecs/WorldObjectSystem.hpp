/**
 * @file
 * @author Max Godefroy
 * @date 12/09/2026.
 */

#pragma once

#include <box3d/box3d.h>
#include <EASTL/vector.h>
#include <KryneEngine/Core/Math/Quaternion.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Core/Memory/Containers/SpscQueue.hpp>
#include <KryneEngine/Core/Memory/GenerationalPool.hpp>
#include <KryneEngine/Core/Memory/SimplePool.hpp>

namespace KryneEngine::Samples
{
    class DrawInstanceManager;
}

namespace KryneEngine::Samples::PhysicsDemo
{
    struct Transform
    {
        float3 m_position {};
        Math::Quaternion m_rotation {};
        float3 m_scale { 1.f, 1.f, 1.f };
    };

    KE_GENPOOL_DECLARE_HANDLE(EntityHandle);

    /**
     * @brief A very simplified ECS: "entities" are just a fixed set of components (a Box3D body
     * and a render instance), registered/unregistered as a whole. There is no generic component
     * storage or query system.
     *
     * @details
     * Threading contract:
     * - CreateEntity/DestroyEntity/Update must only be called from the fixed-step game-loop thread
     *   (SceneManager::GameLoop), or before the render loop starts. This is required by Box3D, which
     *   offers no guarantee that body creation/destruction is safe to call concurrently with
     *   b3World_Step.
     * - SyncRenderInstances must only be called from the main/render thread, once per rendered frame.
     *
     * No data is ever shared/mutated across those two threads directly: all communication flows one
     * way, in order, through the internal event queue. The render thread reconstructs its own
     * double-buffered transform per entity purely by replaying the events it drains, which is what
     * lets SyncRenderInstances interpolate between the last two fixed steps without any locking.
     *
     * Entity slots are reclaimed (and can be handed back out by CreateEntity) only after
     * SyncRenderInstances has drained the corresponding "destroyed" event, mirroring the
     * Free()-then-FlushDeferredFrees() pattern already used by every graphics backend's
     * FlushPools() (see GenerationalPool).
     */
    class WorldObjectSystem
    {
    public:
        explicit WorldObjectSystem(
            AllocatorInstance _allocator,
            b3WorldId _world);
        ~WorldObjectSystem();

        // Creates the Box3D body (using _transform as its initial pose) and queues the registration
        // of a render instance of _renderModel. Call GetBody() right after to attach shapes (and
        // typically b3Body_ApplyMassFromShapes for dynamic bodies).
        [[nodiscard]] EntityHandle CreateEntity(const Transform& _transform, b3BodyDef& _bodyDef, SimplePoolHandle _renderModel);

        [[nodiscard]] b3BodyId GetBody(EntityHandle _entity) const;

        void DestroyEntity(EntityHandle _entity);

        // Must be called once per fixed physics step, right after b3World_Step.
        void Update();

        void FlushEvents(DrawInstanceManager& _drawInstanceManager);

        void SyncRenderInstances(DrawInstanceManager& _drawInstanceManager, float _alpha);

    private:
        struct EntityInternal
        {
            b3BodyId m_body {};
            Transform m_transform {};   // authoritative pose; touched only by the game-loop thread
        };

        struct EntityEvent
        {
            enum class Kind : u8 { Created, Moved, Destroyed };

            Transform m_transform {};       // meaningful for Created/Moved
            SimplePoolHandle m_renderModel {}; // meaningful for Created only
            EntityHandle m_entity {};
            Kind m_kind = Kind::Moved;
        };

        struct RenderEntityState
        {
            Transform m_previous {};
            Transform m_current {};
            SimplePoolHandle m_drawInstance {};
            bool m_registered = false;
        };

        AllocatorInstance m_allocator;
        b3WorldId m_world;

        GenerationalPool<EntityInternal> m_entities;
        eastl::vector<EntityHandle> m_liveHandles; // game-loop-thread-owned only

        // Invariant contract : can only be written to by the game loop, and read by the render loop.
        // The render loop can only access it after ensuring game loop work is done, and before queueing some more.
        eastl::vector<EntityEvent> m_events;

        eastl::vector<RenderEntityState> m_renderStates; // render-thread-owned only
    };
}
