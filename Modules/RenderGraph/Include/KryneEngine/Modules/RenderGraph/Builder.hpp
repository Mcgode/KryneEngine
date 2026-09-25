/**
 * @file
 * @author Max Godefroy
 * @date 13/11/2024.
 */

#pragma once

#include <EASTL/hash_map.h>
#include <EASTL/vector_set.h>
#include <KryneEngine/Core/Memory/SimplePool.hpp>
#include <string>

#include "KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp"

namespace KryneEngine
{
    struct RenderPassDesc;
}

namespace KryneEngine::Modules::RenderGraph
{
    class Registry;
    enum class PassType;

    class Builder
    {
        friend class RenderGraph;
        friend class ResourceStateTracker;
        friend class ImGuiDebugWindow;

    public:
        explicit Builder(Registry& _registry);
        ~Builder();

    public:
        PassDeclarationBuilder DeclarePass(PassType _type);

        Builder& DeclareTargetResource(SimplePoolHandle _resource);

        const PassDeclaration& GetPass(size_t _index) { return m_declaredPasses[_index]; }

        void BuildDag();

    private:
        void ProcessDagDeferredCulling();

        void PrintResource(SimplePoolHandle _resource, std::string& _indent);
        void PrintRenderPassAttachments(const PassDeclaration& _pass, std::string& _indent);
        void PrintDependencies(const PassDeclaration& _pass, std::string& _indent);
        void PrintDag();
        void PrintFlattenedPasses();

    private:
        Registry& m_registry;
        bool m_isBuilt = false;

        eastl::vector<PassDeclaration> m_declaredPasses;
        eastl::hash_map<SimplePoolHandle, eastl::pair<u32, size_t>> m_resourceVersions;

        using VersionedResources = eastl::vector<eastl::pair<SimplePoolHandle, u32>>;
        eastl::vector<VersionedResources> m_versionedReads;
        eastl::vector<VersionedResources> m_versionedWrites;

        struct Node
        {
            eastl::vector_set<size_t> m_children;
            eastl::vector_set<size_t> m_parents;
        };
        eastl::vector<Node> m_dag;

        eastl::vector_set<SimplePoolHandle> m_targetResources;
        eastl::vector<bool> m_passAlive;
    };
} // namespace KryneEngine::Modules::RenderGraph
