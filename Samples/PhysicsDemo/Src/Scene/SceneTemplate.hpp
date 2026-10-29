/**
 * @file
 * @author Max Godefroy
 * @date 22/09/2026.
 */

#pragma once

#include "../Ecs/WorldObjectSystem.hpp"
#include "../Geometry/GeometryLibrary.hpp"

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <KryneEngine/Core/Memory/SimplePool.hpp>
#include <Scene/OrbitCamera.hpp>

namespace KryneEngine::Samples::PhysicsDemo
{
    // One long-lived DrawInstanceManager model per GeometryType, registered once by SceneManager
    // and shared by every template: DrawInstanceManager models are meant to be registered once at
    // setup (there is no UnregisterModel), so templates must look models up here rather than
    // registering their own on every Build().
    using GeometryModelArray = eastl::array<SimplePoolHandle, static_cast<size_t>(GeometryType::Count)>;

    /**
     * @brief Everything a SceneTemplate needs to spawn its content, plus the bookkeeping that
     * lets its caller tear the scene back down again (to reload it, or switch to another
     * template) without the template itself having to track what it created.
     *
     * @details
     * Follows WorldObjectSystem's own threading contract: a template's Build() may only run from
     * the fixed-step game-loop thread (SceneManager::GameLoop), or before the render loop starts.
     *
     * WorldObjectSystem is deliberately kept private here: every entity a template creates must
     * go through CreateEntity() below, so it is fully set up (body, render instance) and gets
     * destroyed with the rest of the scene when reloading or switching templates. Templates may
     * still attach their own components (e.g. tags) through GetRegistry().
     */
    class SceneBuildContext
    {
    public:
        SceneBuildContext(
            GeometryLibrary& _geometryLibrary,
            WorldObjectSystem& _worldObjectSystem,
            const GeometryModelArray& _geometryModels,
            b3WorldId _world,
            OrbitCamera& _orbitCamera)
                : m_geometryLibrary(_geometryLibrary)
                , m_worldObjectSystem(_worldObjectSystem)
                , m_geometryModels(_geometryModels)
                , m_world(_world)
                , m_orbitCamera(_orbitCamera)
        {}

        [[nodiscard]] GeometryLibrary& GetGeometryLibrary() const { return m_geometryLibrary; }

        // Computes a world-space picking ray through the given NDC coordinates (x/y in [-1, 1],
        // y up); see OrbitCamera::GetPickingRay for the threading contract this follows (same as
        // Build()/Process() themselves).
        [[nodiscard]] Math::Ray GetPickingRay(const float2 _ndc) const
        {
            return m_orbitCamera.GetPickingRay(_ndc);
        }

        // Casts a ray against every shape in the physics world and returns the closest hit, if
        // any (check the result's `hit` field). _translation is the ray's end point relative to
        // _origin (i.e. direction * max distance, not a normalized direction).
        [[nodiscard]] b3RayResult CastRayClosest(const float3& _origin, const float3& _translation) const
        {
            return b3World_CastRayClosest(
                m_world,
                b3Pos { _origin.x, _origin.y, _origin.z },
                b3Vec3 { _translation.x, _translation.y, _translation.z },
                b3DefaultQueryFilter());
        }

        // Escape hatch for physics objects that have no render representation and so don't belong
        // in the entities CreateEntity()/DestroyEntity() manage (e.g. joints, or a
        // kinematic body used purely as a joint anchor): a template using this is responsible for
        // destroying whatever it creates through it (typically from its own destructor).
        [[nodiscard]] b3WorldId GetWorld() const { return m_world; }

        [[nodiscard]] SimplePoolHandle GetModel(const GeometryType _type) const
        {
            return m_geometryModels[static_cast<size_t>(_type)];
        }

        [[nodiscard]] b3BodyId GetBody(const entt::entity _entity) const
        {
            return m_worldObjectSystem.GetBody(_entity);
        }

        // Entity owning a body, or entt::null if the body wasn't created through CreateEntity().
        [[nodiscard]] entt::entity GetEntity(const b3BodyId _body) const
        {
            return WorldObjectSystem::GetEntity(_body);
        }

        // Game registry, for templates to attach/query their own components. Entities must still be
        // created and destroyed through CreateEntity()/DestroyEntity().
        [[nodiscard]] Ecs::Registry& GetRegistry() const { return m_worldObjectSystem.GetRegistry(); }

        entt::entity CreateEntity(const Transform& _transform, b3BodyDef& _bodyDef, const SimplePoolHandle _renderModel) const
        {
            return m_worldObjectSystem.CreateEntity(_transform, _bodyDef, _renderModel);
        }

        // Destroys an entity ahead of the caller (SceneManager) tearing down whatever is left when
        // the template itself is unloaded. Lets a template recycle entities (e.g. a ring buffer of
        // spawned objects) without waiting for a full scene swap.
        void DestroyEntity(const entt::entity _entity) const
        {
            m_worldObjectSystem.DestroyEntity(_entity);
        }

    private:
        GeometryLibrary& m_geometryLibrary;
        WorldObjectSystem& m_worldObjectSystem;
        const GeometryModelArray& m_geometryModels;
        b3WorldId m_world;
        OrbitCamera& m_orbitCamera;
    };

    /**
     * @brief A reloadable, self-contained recipe for populating the world with a demo scene
     * (geometry models + physics bodies), so several can coexist and be swapped in/out at will to
     * showcase different physics features.
     *
     * @details
     * Templates may carry their own state (e.g. spawn timers, references to entities they created)
     * to drive custom interactions from Process(). SceneManager only ever keeps at most two
     * instances alive at once - the active one, and one pending swap-in - and owns whichever
     * instances it is given (see SceneManager::RequestLoadScene).
     */
    class SceneTemplate
    {
    public:
        virtual ~SceneTemplate() = default;

        [[nodiscard]] virtual const char* GetName() const = 0;

        // Spawns this template's entities via _context. Only ever adds entities; the caller
        // (SceneManager) is responsible for destroying whatever a previous Build() created before
        // calling this again.
        virtual void Build(SceneBuildContext& _context) = 0;

        // Called once per fixed physics step, right before the physics world is stepped, while
        // this template is the active one. Lets a template drive custom interactions (spawning
        // entities, applying forces, reacting to input, ...); default is a no-op, since most
        // templates are static once built. Follows the same threading contract as Build().
        virtual void Process(SceneBuildContext& _context, float _deltaTime) {}
    };
}
