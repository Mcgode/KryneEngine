/**
 * @file
 * @author Max Godefroy
 * @date 04/08/2024.
 */

#pragma once

#include <KryneEngine/Core/Graphics/Buffer.hpp>
#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Graphics/Handles.hpp>
#include <KryneEngine/Core/Graphics/MemoryBarriers.hpp>
#include <KryneEngine/Core/Memory/DynamicArray.hpp>

namespace KryneEngine::Modules::GraphicsUtils
{
    class DynamicBuffer
    {
    public:
        explicit DynamicBuffer(AllocatorInstance _allocator);

        /**
         * @brief Creates one CPU-mappable buffer per frame, plus a device-local buffer if requested and beneficial.
         *
         * @param _bufferDesc Description of the buffer. Its usage type must be `StageEveryFrame_UsageType` or
         * `CpuReadWrite_UsageType`.
         * @param _makeGpuReadOptimal If `true` and the GPU doesn't read CPU-visible memory at full speed, the GPU
         * reads a device-local buffer that #PrepareBuffers fills from the mappable buffers. Has no effect otherwise.
         *
         * @warning `WriteBuffer` usage is not supported: the buffer is written by the CPU only, and GPU writes would
         * not reach the mappable buffers when a device-local buffer is used.
         */
        void Init(
            GraphicsContext* _graphicsContext,
            const BufferCreateDesc& _bufferDesc,
            u8 _frameCount,
            bool _makeGpuReadOptimal = true);
        void RequestResize(u64 _size);
        void* Map(GraphicsContext* _graphicsContext, u8 _frameIndex);
        void Unmap(GraphicsContext* _graphicsContext);

        /**
         * @brief Gets a CPU pointer to the start of the frame's mappable buffer, as an alternative to #Map and #Unmap.
         *
         * @details Applies any pending resize requested through #RequestResize, like #Map does. The pointer stays
         * valid until the next call to #Map or #GetPersistentPointer for the same frame index, which can recreate the
         * buffer, so it can be kept and reused as long as no resize is requested. Writes must be followed by
         * #FlushPersistent before the frame's GPU work is submitted.
         *
         * @warning Don't use it between a #Map and its matching #Unmap.
         */
        [[nodiscard]] std::byte* GetPersistentRawPointer(GraphicsContext* _graphicsContext, u8 _frameIndex);

        template <class T>
        [[nodiscard]] T* GetPersistentPointer(GraphicsContext* _graphicsContext, const u8 _frameIndex)
        {
            return reinterpret_cast<T*>(GetPersistentRawPointer(_graphicsContext, _frameIndex));
        }

        /**
         * @brief Makes CPU writes done through #GetPersistentPointer visible to the GPU.
         *
         * @param _size Size in bytes of the flushed range. `~0ull` flushes up to the end of the buffer.
         */
        void FlushPersistent(
            GraphicsContext* _graphicsContext,
            u8 _frameIndex,
            u64 _offset = 0,
            u64 _size = ~0ull) const;

        void PrepareBuffers(
            GraphicsContext* _graphicsContext,
            TransferCommandEncoderHandle _transferEncoder,
            BarrierAccessFlags _accessFlags,
            u8 _frameIndex) const;

        /**
         * @brief Informs you whether the GPU reads the CPU-mappable buffers directly, or reads a device-local buffer
         * that the mappable buffers are copied into by #PrepareBuffers.
         *
         * @returns `true` if the final buffer is the mappable one, `false` if there are intermediate buffers for
         * data transfer.
         */
        [[nodiscard]] bool IsDirectMapping() const { return m_gpuBuffer == GenPool::kInvalidHandle; }

        [[nodiscard]] u64 GetSize(const u8 _frameIndex) const
        {
            return m_sizes[_frameIndex];
        }

        BufferHandle GetBuffer(u8 _frameIndex);

        void Destroy(GraphicsContext* _graphicsContext);

        [[nodiscard]] bool NeedsInit() const { return m_mappableBuffers.Empty(); }

    private:
        void ApplyPendingResize(GraphicsContext* _graphicsContext, u8 _frameIndex);

        BufferCreateDesc m_mappableRecreateDesc {};
        BufferCreateDesc m_gpuRecreateDesc {};
        DynamicArray<BufferHandle> m_mappableBuffers;
        DynamicArray<u64> m_sizes;
        BufferHandle m_gpuBuffer { GenPool::kInvalidHandle };
        BufferMapping m_currentMapping { { GenPool::kInvalidHandle } };

        struct BufferToFree
        {
            BufferHandle m_buffer;
            u8 m_atIndex = 0;
        };
        eastl::vector<BufferToFree> m_gpuBuffersToFree;
    };
}