/**
 * @file
 * @author Max Godefroy
 * @date 14/11/2024.
 */

#include "KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp"

namespace KryneEngine::Modules::RenderGraph
{
    PassDeclaration::PassDeclaration(PassType _type, size_t _id)
        : m_type(_type)
        , m_name(eastl::string().sprintf("Pass %zu", _id))
    {}

    u64 PassDeclaration::GetRenderPassHash()
    {
        if (!m_renderPassHash.has_value())
        {
            u64 hash = Hashing::Hash64<const PassAttachmentDeclaration>(m_colorAttachments);
            if (m_depthAttachment.has_value())
            {
                hash = Hashing::Hash64Append(&m_depthAttachment.value(), hash);
            }
            m_renderPassHash.emplace(hash);
        }

        return m_renderPassHash.value();
    }

    PassDeclarationBuilder& PassDeclarationBuilder::SetName(const eastl::string_view& _name)
    {
        m_item.m_name = StringHash(_name);
        return *this;
    }

    PassAttachmentDeclarationBuilder PassDeclarationBuilder::AddColorAttachment(SimplePoolHandle _texture)
    {
        return { m_item.m_colorAttachments.emplace_back(_texture), this };
    }

    PassAttachmentDeclarationBuilder PassDeclarationBuilder::SetDepthAttachment(SimplePoolHandle _texture)
    {
        m_item.m_depthAttachment.emplace(_texture);
        m_item.m_depthAttachment.value().m_clearDepth = 1.f;
        m_item.m_depthAttachment.value().m_clearStencil = ~0;
        m_item.m_depthAttachment.value().m_stencilLoadOperation = RenderPassDesc::Attachment::LoadOperation::DontCare;
        m_item.m_depthAttachment.value().m_stencilStoreOperation = RenderPassDesc::Attachment::StoreOperation::DontCare;
        m_item.m_depthAttachment.value().m_readOnly = false;
        return { m_item.m_depthAttachment.value(), this};
    }

    PassDeclarationBuilder& PassDeclarationBuilder::ReadDependency(const Dependency& _dependency)
    {
        m_item.m_readDependencies.push_back(_dependency);
        return *this;
    }

    PassDeclarationBuilder& PassDeclarationBuilder::WriteDependency(const Dependency& _dependency)
    {
        m_item.m_writeDependencies.emplace_back(_dependency);
        return *this;
    }

    PassDeclarationBuilder& PassDeclarationBuilder::SetExecuteFunction(PassDeclaration::ExecuteFunction&& _function)
    {
        m_item.m_executeFunction = _function;
        return *this;
    }
}