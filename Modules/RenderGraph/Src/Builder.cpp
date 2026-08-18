/**
 * @file
 * @author Max Godefroy
 * @date 13/11/2024.
 */

#include "KryneEngine/Modules/RenderGraph/Builder.hpp"

#include <iostream>
#include <KryneEngine/Core/Memory/SimplePool.inl>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>

#include "KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp"
#include "KryneEngine/Modules/RenderGraph/Registry.hpp"
#include "KryneEngine/Modules/RenderGraph/Resource.hpp"

namespace KryneEngine::Modules::RenderGraph
{
    Builder::Builder(Registry& _registry)
        : m_registry(_registry)
    {}

    Builder::~Builder() = default;

    PassDeclarationBuilder Builder::DeclarePass(PassType _type)
    {
        return { m_declaredPasses.emplace_back(_type, m_declaredPasses.size()), this };
    }

    Builder& Builder::DeclareTargetResource(SimplePoolHandle _resource)
    {
        const SimplePoolHandle underlyingResource = m_registry.GetUnderlyingResource(_resource);
        m_targetResources.insert(underlyingResource);
        return *this;
    }

    void Builder::BuildDag()
    {
        if (m_isBuilt)
            return;

        KE_ZoneScopedFunction("Builder::BuildDag");

        m_dag.resize(m_declaredPasses.size());
        m_passAlive.resize(m_declaredPasses.size(), false);

        m_isBuilt = true;

        const auto handleResourceRead = [this](SimplePoolHandle _resource, size_t _index)
        {
            const auto versionIt = m_resourceVersions.find(m_registry.GetUnderlyingResource(_resource));
            if (versionIt != m_resourceVersions.end())
            {
                m_dag[versionIt->second.second].m_children.insert(_index);
                m_dag[_index].m_parents.insert(versionIt->second.second);
            }
        };

        const auto handleResourceWrite = [this](SimplePoolHandle _resource, size_t _index)
        {
            m_resourceVersions[_resource].first++;
            m_resourceVersions[_resource].second = _index;

            if (m_targetResources.find(_resource) != m_targetResources.end())
            {
                m_passAlive[_index] = true;
            }
        };

        for (auto i = 0; i < m_declaredPasses.size(); i++)
        {
            const PassDeclaration& passDeclaration = m_declaredPasses[i];

            for (const Dependency& dependency: passDeclaration.m_readDependencies)
            {
                handleResourceRead(dependency.m_resource, i);
            }
            for (const Dependency& dependency: passDeclaration.m_writeDependencies)
            {
                handleResourceWrite(m_registry.GetUnderlyingResource(dependency.m_resource), i);
            }

            // Render targets are to be marked as implicit READ/WRITE dependencies
            if (passDeclaration.m_type == PassType::Render)
            {
                for (const auto& colorAttachment: passDeclaration.m_colorAttachments)
                {
                    handleResourceRead(colorAttachment.m_rtv, i);
                    handleResourceWrite(m_registry.GetUnderlyingResource(colorAttachment.m_rtv), i);
                }
                if (passDeclaration.m_depthAttachment.has_value())
                {
                    handleResourceRead(passDeclaration.m_depthAttachment->m_rtv, i);
                    handleResourceWrite(m_registry.GetUnderlyingResource(passDeclaration.m_depthAttachment->m_rtv), i);
                }
            }
        }

        ProcessDagDeferredCulling();
    }

    void Builder::ProcessDagDeferredCulling()
    {
        KE_ZoneScopedFunction("Builder::ProcessDagDeferredCulling");

        for (size_t i = 0; i < m_passAlive.size(); i++)
        {
            const size_t index = m_passAlive.size() - i - 1;
            if (m_passAlive[index])
            {
                for (const size_t parent: m_dag[index].m_parents)
                {
                    m_passAlive[parent] = true;
                }
            }
        }
    }

    void Builder::PrintResource(SimplePoolHandle _resource, std::string& _indent)
    {
        KE_ZoneScopedFunction("Builder::PrintResource");

        const Resource& resource = m_registry.m_resources.Get(_resource);

        std::cout << _indent << "- ";

#if !defined(KE_FINAL)
        if (!resource.m_name.empty())
        {
            std::cout << eastl::string().sprintf("'%s'", resource.m_name.c_str()).c_str();
        }
        else
#endif
        {
            std::cout << "Resource " << _resource;
        }

        switch (resource.m_type)
        {
        case ResourceType::RawTexture:
            std::cout
                << eastl::string().sprintf(", Raw texture, handle: Ox%x",(u32)resource.m_rawTextureData.m_texture.m_handle).c_str()
                << std::endl;
            break;
        case ResourceType::Buffer:
            std::cout
                << eastl::string().sprintf(", Raw buffer, handle: Ox%x",(u32)resource.m_rawTextureData.m_texture.m_handle).c_str()
                << std::endl;
            break;
        case ResourceType::Sampler:
            std::cout
                << eastl::string().sprintf(", Sampler, handle: Ox%x",(u32)resource.m_rawTextureData.m_texture.m_handle).c_str()
                << std::endl;
            break;
        case ResourceType::TextureView:
            std::cout
                << eastl::string().sprintf(", Texture View, handle: Ox%x",(u32)resource.m_textureViewData.m_textureView.m_handle).c_str()
                << std::endl;
            break;
        case ResourceType::BufferView:
            std::cout
                << eastl::string().sprintf(", Buffer View, handle: Ox%x",(u32)resource.m_bufferViewData.m_bufferView.m_handle).c_str()
                << std::endl;
            break;
        case ResourceType::RenderTargetView:
            std::cout
                << eastl::string().sprintf(", Render target view, handle: Ox%x",(u32)resource.m_renderTargetViewData.m_renderTargetView.m_handle).c_str()
                << std::endl;
            break;
        case ResourceType::Dummy:
            std::cout
                << ", Dummy"
                << std::endl;
            break;
        }
    }

    void Builder::PrintRenderPassAttachments(const PassDeclaration& _pass, std::string& _indent)
    {
        KE_ZoneScopedFunction("Builder::PrintRenderPassAttachments");

        if (!_pass.m_colorAttachments.empty())
        {
            std::cout << _indent << "Color attachments:" << std::endl;
            _indent.push_back('\t');
            for (auto& attachment : _pass.m_colorAttachments)
            {
                PrintResource(attachment.m_rtv, _indent);
                _indent.push_back('\t');
                std::cout << _indent;
                switch (attachment.m_loadOperation)
                {
                case RenderPassDesc::Attachment::LoadOperation::Load:
                    std::cout << "Load operation: LOAD; ";
                    break;
                case RenderPassDesc::Attachment::LoadOperation::Clear:
                    std::cout << "Load operation: CLEAR; ";
                    break;
                case RenderPassDesc::Attachment::LoadOperation::DontCare:
                    std::cout << "Load operation: DONT_CARE; ";
                    break;
                }
                switch (attachment.m_storeOperation)
                {
                case RenderPassDesc::Attachment::StoreOperation::Store:
                    std::cout << "Store operation: STORE";
                    break;
                case RenderPassDesc::Attachment::StoreOperation::Resolve:
                    std::cout << "Store operation: RESOLVE";
                    break;
                case RenderPassDesc::Attachment::StoreOperation::DontCare:
                    std::cout << "Store operation: DONT_CARE";
                    break;
                }
                _indent.pop_back();
                std::cout << std::endl;
            }
            _indent.pop_back();
        }

        if (_pass.m_depthAttachment.has_value())
        {
            std::cout << _indent << "Depth/stencil attachment:" << std::endl;
            _indent.push_back('\t');
            PrintResource(_pass.m_depthAttachment->m_rtv, _indent);
            _indent.push_back('\t');
            std::cout << _indent;
            switch (_pass.m_depthAttachment.value().m_loadOperation)
            {
            case RenderPassDesc::Attachment::LoadOperation::Load:
                std::cout << "Load operation: LOAD; ";
                break;
            case RenderPassDesc::Attachment::LoadOperation::Clear:
                std::cout << "Load operation: CLEAR; ";
                break;
            case RenderPassDesc::Attachment::LoadOperation::DontCare:
                std::cout << "Load operation: DONT_CARE; ";
                break;
            }
            switch (_pass.m_depthAttachment.value().m_storeOperation)
            {
            case RenderPassDesc::Attachment::StoreOperation::Store:
                std::cout << "Store operation: STORE;";
                break;
            case RenderPassDesc::Attachment::StoreOperation::Resolve:
                std::cout << "Store operation: RESOLVE;";
                break;
            case RenderPassDesc::Attachment::StoreOperation::DontCare:
                std::cout << "Store operation: DONT_CARE;";
                break;
            }
            std::cout << std::endl;
            _indent.pop_back();
            _indent.pop_back();
        }
    }

    void Builder::PrintDependencies(const PassDeclaration& _pass, std::string& _indent)
    {
        KE_ZoneScopedFunction("Builder::PrintDependencies");

        if (!_pass.m_readDependencies.empty())
        {
            std::cout << _indent << "Read dependencies:" << std::endl;
            _indent.push_back('\t');
            for (const Dependency& dependency: _pass.m_readDependencies)
            {
                const auto versionIt = m_resourceVersions.find(m_registry.GetUnderlyingResource(dependency.m_resource));
                const u32 version = versionIt != m_resourceVersions.end() ? versionIt->second.first : 0;

                PrintResource(dependency.m_resource, _indent);
                _indent.push_back('\t');
                std::cout << _indent << "Version " << version << std::endl;
                _indent.pop_back();
            }
            _indent.pop_back();
        }

        if (!_pass.m_writeDependencies.empty())
        {
            std::cout << _indent << "Write dependencies:" << std::endl;
            _indent.push_back('\t');
            for (const Dependency& dependency: _pass.m_writeDependencies)
            {
                const auto versionIt = m_resourceVersions.find(dependency.m_resource);
                const u32 version = versionIt != m_resourceVersions.end() ? versionIt->second.first : 0;

                PrintResource(dependency.m_resource, _indent);
                _indent.push_back('\t');
                std::cout << _indent << "Version " << version << std::endl;
                _indent.pop_back();
            }
            _indent.pop_back();
        }
    }

    void Builder::PrintDag()
    {
        KE_ZoneScopedFunction("Builder::PrintDag");

        std::cout << std::endl;
        std::cout << "DAG:" << std::endl;
        std::cout << "digraph RawRenderGraph {" << std::endl;

        for (size_t i = 0; i < m_dag.size(); i++)
        {
            const Node& node = m_dag[i];
            for (size_t child: node.m_children)
            {
                std::cout
                    << eastl::string{}.sprintf(R"(  "[%lld] %s" -> "[%lld] %s";)",
                                               i, m_declaredPasses[i].m_name.m_string.c_str(),
                                               child, m_declaredPasses[child].m_name.m_string.c_str()).c_str()
                    << std::endl;
            }
            if (node.m_children.empty())
            {
                std::cout
                    << eastl::string{}.sprintf(R"(  "[%lld] %s";)", i, m_declaredPasses[i].m_name.m_string.c_str()).c_str()
                    << std::endl;
            }
        }
        std::cout << "}" << std::endl;std::cout << std::endl;
        std::cout << "Culled DAG:" << std::endl;
        std::cout << "digraph RenderGraph {" << std::endl;

        for (size_t i = 0; i < m_dag.size(); i++)
        {
            if (!m_passAlive[i])
            {
                continue;
            }

            const Node& node = m_dag[i];
            for (size_t child: node.m_children)
            {
                if (!m_passAlive[child])
                {
                    continue;
                }
                std::cout
                    << eastl::string{}.sprintf(R"(  "[%lld] %s" -> "[%lld] %s";)",
                                               i, m_declaredPasses[i].m_name.m_string.c_str(),
                                               child, m_declaredPasses[child].m_name.m_string.c_str()).c_str()
                    << std::endl;
            }
            if (node.m_children.empty())
            {
                std::cout
                    << eastl::string{}.sprintf(R"(  "[%lld] %s";)", i, m_declaredPasses[i].m_name.m_string.c_str()).c_str()
                    << std::endl;
            }
        }
        std::cout << "}" << std::endl;
    }

    void Builder::PrintFlattenedPasses()
    {
        KE_ZoneScopedFunction("Builder::PrintFlattenedPasses");

        eastl::vector<size_t> renderPasses;
        eastl::vector<size_t> computePasses;
        eastl::vector<size_t> transferPasses;

        constexpr auto typeCount = static_cast<size_t>(PassType::COUNT);
        eastl::vector<eastl::pair<size_t, size_t>> crossQueueDependencyMatrix[typeCount * typeCount];

        for (size_t i = 0; i < m_declaredPasses.size(); i++)
        {
            const PassDeclaration& pass = m_declaredPasses[i];
            if (!m_passAlive[i])
            {
                continue;
            }
            switch (pass.m_type)
            {
            case PassType::Render:
                renderPasses.push_back(i);
                break;
            case PassType::Compute:
                computePasses.push_back(i);
                break;
            case PassType::Transfer:
                transferPasses.push_back(i);
                break;
            case PassType::COUNT:
                continue;
            }

            const auto passTypeOffset = static_cast<size_t>(pass.m_type) * typeCount;
            for (size_t childIndex : m_dag[i].m_children)
            {
                if (!m_passAlive[childIndex])
                {
                    continue;
                }
                const PassDeclaration& child = m_declaredPasses[childIndex];
                if (child.m_type == pass.m_type)
                {
                    continue;
                }
                const auto childType = static_cast<size_t>(child.m_type);
                const size_t crossDepIndex = childType + passTypeOffset;
                auto& dependencies = crossQueueDependencyMatrix[crossDepIndex];
                if (dependencies.empty() || dependencies.back().first != i)
                {
                    dependencies.emplace_back(i, childIndex);
                }
            }
        }

        std::cout << std::endl;
        std::cout << "Flattened passes:" << std::endl;
        std::cout << "digraph FlattenedPasses {" << std::endl;

        std::cout << "\tsubgraph RenderPasses {" << std::endl;
        if (renderPasses.size() == 1)
        {
            std::cout
                << "\t\t"
                << eastl::string().sprintf(
                                      R"("[%lld] %s";)",
                                      renderPasses[0],
                                      m_declaredPasses[renderPasses[0]].m_name.m_string.c_str()).c_str()
                << std::endl;
        }
        for (size_t i = 1; i < renderPasses.size(); i++)
        {
            std::cout
                << "\t\t"
                << eastl::string().sprintf(
                                      R"("[%lld] %s" -> "[%lld] %s";)",
                                      renderPasses[i - 1],
                                      m_declaredPasses[renderPasses[i - 1]].m_name.m_string.c_str(),
                                      renderPasses[i],
                                      m_declaredPasses[renderPasses[i]].m_name.m_string.c_str()).c_str()
                << std::endl;
        }
        std::cout << "\t}" << std::endl;

        std::cout << "\tsubgraph ComputePasses {" << std::endl;
        if (computePasses.size() == 1)
        {
            std::cout
                << "\t\t"
                << eastl::string().sprintf(
                                      R"("[%lld] %s";)",
                                      computePasses[0],
                                      m_declaredPasses[computePasses[0]].m_name.m_string.c_str()).c_str()
                << std::endl;
        }
        for (size_t i = 1; i < computePasses.size(); i++)
        {
            std::cout
                << "\t\t"
                << eastl::string().sprintf(
                                      R"("[%lld] %s" -> "[%lld] %s";)",
                                      computePasses[i - 1],
                                      m_declaredPasses[computePasses[i - 1]].m_name.m_string.c_str(),
                                      computePasses[i],
                                      m_declaredPasses[computePasses[i]].m_name.m_string.c_str()).c_str()
                << std::endl;
        }
        std::cout << "\t}" << std::endl;

        std::cout << "\tsubgraph TransferPasses {" << std::endl;
        if (transferPasses.size() == 1)
        {
            std::cout
                << "\t\t"
                << eastl::string().sprintf(
                                      R"("[%lld] %s";)",
                                      transferPasses[0],
                                      m_declaredPasses[transferPasses[0]].m_name.m_string.c_str()).c_str()
                << std::endl;
        }
        for (size_t i = 1; i < transferPasses.size(); i++)
        {
            std::cout
                << "\t\t"
                << eastl::string().sprintf(
                                      R"("[%lld] %s" -> "[%lld] %s";)",
                                      transferPasses[i - 1],
                                      m_declaredPasses[transferPasses[i - 1]].m_name.m_string.c_str(),
                                      transferPasses[i],
                                      m_declaredPasses[transferPasses[i]].m_name.m_string.c_str()).c_str()
                << std::endl;
        }
        std::cout << "\t}" << std::endl;

        for (const auto& crossQueueDependencies : crossQueueDependencyMatrix)
        {
            for (const auto& dependencyPair : crossQueueDependencies)
            {
                std::cout
                    << "\t"
                    << eastl::string().sprintf(
                                          R"("[%lld] %s" -> "[%lld] %s";)",
                                          dependencyPair.first,
                                          m_declaredPasses[dependencyPair.first].m_name.m_string.c_str(),
                                          dependencyPair.second,
                                          m_declaredPasses[dependencyPair.second].m_name.m_string.c_str()).c_str()
                    << std::endl;
            }
        }

        std::cout << "}" << std::endl;
    }
} // namespace KryneEngine::Modules::RenderGraph
