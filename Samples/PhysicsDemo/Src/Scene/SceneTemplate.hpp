/**
 * @file
 * @author Max Godefroy
 * @date 22/09/2026.
 */

#pragma once

#include "../Ecs/WorldObjectSystem.hpp"
#include "../Geometry/GeometryLibrary.hpp"

#include <EASTL/algorithm.h>
#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <KryneEngine/Core/Memory/SimplePool.hpp>

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
     * go through CreateEntity() below so the caller can always find (and destroy) it again when
     * reloading or switching templates. A template only ever needs to create/query entities and
     * look up shared models, never the WorldObjectSystem itself.
     */
    class SceneBuildContext
    {
    public:
        SceneBuildContext(
            GeometryLibrary& _geometryLibrary,
            WorldObjectSystem& _worldObjectSystem,
            const GeometryModelArray& _geometryModels,
            eastl::vector<EntityHandle>& _createdEntities)
                : m_geometryLibrary(_geometryLibrary)
                , m_worldObjectSystem(_worldObjectSystem)
                , m_geometryModels(_geometryModels)
                , m_createdEntities(_createdEntities)
        {}

        [[nodiscard]] GeometryLibrary& GetGeometryLibrary() const { return m_geometryLibrary; }

        [[nodiscard]] SimplePoolHandle GetModel(const GeometryType _type) const
        {
            return m_geometryModels[static_cast<size_t>(_type)];
        }

        [[nodiscard]] b3BodyId GetBody(const EntityHandle _entity) const
        {
            return m_worldObjectSystem.GetBody(_entity);
        }

        EntityHandle CreateEntity(const Transform& _transform, b3BodyDef& _bodyDef, const SimplePoolHandle _renderModel) const
        {
            const EntityHandle entity = m_worldObjectSystem.CreateEntity(_transform, _bodyDef, _renderModel);
            m_createdEntities.push_back(entity);
            return entity;
        }

        // Destroys an entity a previous CreateEntity() call on this same context returned, ahead
        // of the caller (SceneManager) tearing down whatever is left when the template itself is
        // unloaded. Lets a template recycle entities (e.g. a ring buffer of spawned objects)
        // without waiting for a full scene swap.
        void DestroyEntity(const EntityHandle _entity) const
        {
            m_worldObjectSystem.DestroyEntity(_entity);
            const auto it = eastl::find(m_createdEntities.begin(), m_createdEntities.end(), _entity);
            if (it != m_createdEntities.end())
            {
                m_createdEntities.erase_unsorted(it);
            }
        }

    private:
        GeometryLibrary& m_geometryLibrary;
        WorldObjectSystem& m_worldObjectSystem;
        const GeometryModelArray& m_geometryModels;
        eastl::vector<EntityHandle>& m_createdEntities;
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
