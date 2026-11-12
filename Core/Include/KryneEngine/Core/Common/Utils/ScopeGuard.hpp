/**
 * @file
 * @author Max Godefroy
 * @date 09/11/2026.
 */

#pragma once

#include "Macros.hpp"


#include <type_traits>
#include <utility>

namespace KryneEngine
{
    template <class Callback>
    requires std::is_invocable_r_v<void, Callback>
    class ScopeGuard
    {
    public:
        explicit ScopeGuard(Callback&& _functor) : m_callback(std::move(_functor)) {}
        // Force-inlined so the unwind path does not keep the guard alive on the stack
        KE_FORCEINLINE ~ScopeGuard() { if (m_active) m_callback(); }

        KE_DEFINE_COPY_MOVE_SEMANTICS(ScopeGuard, delete, delete);

        void Cancel() { m_active = false; }

    private:
        Callback m_callback;
        bool m_active = true;
    };
}