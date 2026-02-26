/**
 * @file
 * @author Max Godefroy
 * @date 03/07/2022.
 */

#pragma once

#include <atomic>
#include "KryneEngine/Core/Threads/HelperFunctions.hpp"

namespace KryneEngine
{
    /// @see https://rigtorp.se/spinlock/
    struct SpinLock
    {
    public:
        void Lock() noexcept;

        void Unlock() noexcept;

        [[nodiscard]] bool TryLock() noexcept;
        [[nodiscard]] bool TryLock(u32 _spinCount) noexcept;

        [[nodiscard]] bool IsLocked() const noexcept;

    private:
        std::atomic<bool> m_lock = false;

    public:
        using LockGuardT = Threads::SyncLockGuard<SpinLock, &SpinLock::Lock, &SpinLock::Unlock>;

        [[nodiscard]] LockGuardT AutoLock() noexcept
        {
            return LockGuardT(this);
        }
    };
} // KryneEngine