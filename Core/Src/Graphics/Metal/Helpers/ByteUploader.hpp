/**
 * @file
 * @author Max Godefroy
 * @date 24/08/2026.
 */

#pragma once

#include "KryneEngine/Core/Memory/DynamicArray.hpp"
#include "KryneEngine/Core/Threads/SpinLock.hpp"

#include <EASTL/span.h>
#include <EASTL/vector.h>
#include <Metal/MTLBuffer.hpp>


namespace KryneEngine
{
    /**
     * @brief A per-frame bump allocator for uploading small chunks of data.
     */
    class ByteUploader
    {
    public:
        struct alignas(16) PageHeader
        {
            MTL::Buffer* m_buffer = nullptr;
            PageHeader* m_next = nullptr;
            size_t m_index = sizeof(PageHeader);
        };

        ByteUploader(AllocatorInstance _allocator, size_t _frameContextCount);
        ~ByteUploader();

        [[nodiscard]] MTL::GPUAddress SetBytes(
            MTL::Device* _device,
            eastl::span<const std::byte> _bytes,
            size_t _frameIndex);

        template <class T>
        [[nodiscard]] MTL::GPUAddress SetBytes(
            MTL::Device* _device,
            eastl::span<const T> _bytes,
            size_t _frameIndex)
        {
            return SetBytes(
                _device,
                { reinterpret_cast<const std::byte*>(_bytes.data()), _bytes.size_bytes() },
                _frameIndex);
        }

        void Reset(size_t _frameIndex);

        static constexpr size_t kPageSize = 64 * 1024;

    private:
        eastl::vector<PageHeader*> m_pages;
        eastl::vector<PageHeader*> m_freePages;
        DynamicArray<PageHeader*> m_frameContextPages;
        SpinLock m_lock;
    };
}
