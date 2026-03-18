/**
 * @file
 * @author Max Godefroy
 * @date 27/02/2026.
 */

#pragma once

#include <clay.h>
#include <KryneEngine/Core/Common/Utils/Macros.hpp>

namespace KryneEngine
{
    template <class String>
    Clay_String ToClayString(const String& _string)
    {
        return Clay_String {
            .isStaticallyAllocated = false,
            .length = static_cast<int>(_string.size()),
            .chars = _string.data(),
        };
    }
}
