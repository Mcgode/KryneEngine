/**
 * @file
 * @author Max Godefroy
 * @date 24/08/2026.
 */

#include "ByteUploader.hpp"

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Common/Utils/Alignment.hpp"
#include "Metal/MTLDevice.hpp"

namespace KryneEngine {
    ByteUploader::ByteUploader(AllocatorInstance _allocator, size_t _frameContextCount)
        : m_pages(_allocator)
        , m_freePages(_allocator)
        , m_frameContextPages(_allocator, _frameContextCount, nullptr)
    {}

    ByteUploader::~ByteUploader()
    {
        for (const auto* page: m_pages)
        {
            page->m_buffer->release();
        }
    }

    MTL::GPUAddress ByteUploader::SetBytes(
        MTL::Device* _device,
        const eastl::span<const std::byte> _bytes,
        const size_t _frameIndex)
    {
        const auto lock = m_lock.AutoLock();

        const size_t size = Alignment::AlignUp(_bytes.size(), 16uz);

        KE_ASSERT(size <= kPageSize - sizeof(PageHeader));

        PageHeader*& page = m_frameContextPages[_frameIndex];

        if (page == nullptr || page->m_index + size > kPageSize)
        {
            PageHeader* newPage = nullptr;
            if (m_freePages.empty())
            {
                MTL::Buffer* pageBuffer = _device->newBuffer(kPageSize, MTL::ResourceStorageModeShared);
                newPage = static_cast<PageHeader*>(pageBuffer->contents());
                newPage->m_buffer = pageBuffer;
                newPage->m_index = sizeof(PageHeader);
                m_pages.push_back(newPage);
            }
            else
            {
                newPage = m_freePages.back();
                m_freePages.pop_back();
            }

            newPage->m_next = page;
            page = newPage;
        }

        memcpy(reinterpret_cast<std::byte*>(page) + page->m_index, _bytes.data(), _bytes.size());
        page->m_index += size;
        return page->m_buffer->gpuAddress() + page->m_index - size;
    }


    void ByteUploader::Reset(const size_t _frameIndex)
    {
        PageHeader* page = m_frameContextPages[_frameIndex];
        while (page != nullptr)
        {
            m_freePages.push_back(page);
            page->m_index = sizeof(PageHeader);
            PageHeader* next = page->m_next;
            page->m_next = nullptr;
            page = next;
        }

        m_frameContextPages[_frameIndex] = nullptr;
    }
}