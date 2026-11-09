/**
 * @file
 * @author Max Godefroy
 * @date 09/11/2026.
 */

#pragma once

#include <algorithm>

namespace KryneEngine
{
    template <class Callback>
    requires std::is_invocable_r_v<void, Callback>
    class ScopeGuard
    {
    public:
        explicit ScopeGuard(Callback&& _functor) : m_callback(std::move(_functor)) {}
        ~ScopeGuard() { m_callback(); }

    private:
        Callback m_callback;
    };
}