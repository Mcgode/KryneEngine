/**
 * @file
 * @author Max Godefroy
 * @date 13/11/2026.
 */

#pragma once

#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Modules/Resources/ResourceEntry.hpp>

#include "IResourceManager.hpp"

namespace KryneEngine::Modules::Resources
{
    template <class Resource>
    class ResourceRef
    {
    public:
        ResourceRef() = default;

        explicit ResourceRef(ResourceEntry* _entry)
        {
            if (_entry != nullptr && _entry->TryUseResource())
            {
                m_entry = _entry;
                m_cachedResource = m_entry->GetResource<Resource>();
            }
        }

        ~ResourceRef()
        {
            if (m_entry != nullptr)
                m_entry->m_manager->ReleaseResource(m_entry);
        }

        ResourceRef(const ResourceRef& _other)
            : ResourceRef(_other.m_entry)
        {}

        ResourceRef& operator=(const ResourceRef& _other)
        {
            if (this != &_other)
            {
                if (m_entry != nullptr)
                {
                    m_entry->m_manager->ReleaseResource(m_entry);
                    m_entry = nullptr;
                    m_cachedResource = nullptr;
                }

                if (_other.m_entry != nullptr && _other.m_entry->TryUseResource())
                {
                    m_entry = _other.m_entry;
                    m_cachedResource = _other.m_cachedResource;
                }
            }
            return *this;
        }

        ResourceRef(ResourceRef&& _other) noexcept
            : m_entry(_other.m_entry)
            , m_cachedResource(_other.m_cachedResource)
        {
            _other.m_entry = nullptr;
            _other.m_cachedResource = nullptr;
        }

        ResourceRef& operator=(ResourceRef&& _other) noexcept
        {
            if (this != &_other)
            {
                if (m_entry != nullptr)
                    m_entry->m_manager->ReleaseResource(m_entry);
                m_entry = _other.m_entry;
                m_cachedResource = _other.m_cachedResource;
                _other.m_entry = nullptr;
                _other.m_cachedResource = nullptr;
            }
            return *this;
        }

        Resource* FetchResource(bool _force = false)
        {
            if (IsValid() && (_force || m_cachedResource == nullptr))
                m_cachedResource = m_entry->GetResource<Resource>();
            return m_cachedResource;
        }

        [[nodiscard]] Resource* GetCachedResource() const { return m_cachedResource; }

        [[nodiscard]] bool IsValid() const { return m_entry != nullptr; }

        static ResourceRef FromExistingRef(ResourceEntry* _entry)
        {
            VERIFY_OR_RETURN(_entry != nullptr, {});
            KE_ASSERT(_entry->m_refCount.load(std::memory_order::relaxed) > 0);
            ResourceRef ref;
            ref.m_entry = _entry;
            ref.m_cachedResource = _entry->GetResource<Resource>();
            return ref;
        }

        [[nodiscard]] bool operator==(const ResourceRef& _rhs) const { return m_entry == _rhs.m_entry; }
        [[nodiscard]] bool operator<(const ResourceRef& _rhs) const { return m_entry < _rhs.m_entry; }

    private:
        ResourceEntry* m_entry = nullptr;
        Resource* m_cachedResource = nullptr;
    };
}