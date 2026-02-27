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

    KE_FORCEINLINE bool ClayIsHovering(const Clay_ElementId& _id, const float2& _mousePosition)
    {
        const Clay_ElementData elementData = Clay_GetElementData(_id);
        const float2 topLeft { elementData.boundingBox.x, elementData.boundingBox.y };
        const float2 bottomRight { elementData.boundingBox.x + elementData.boundingBox.width, elementData.boundingBox.y + elementData.boundingBox.height };

        return _mousePosition.x >= topLeft.x
            && _mousePosition.x <= bottomRight.x
            && _mousePosition.y >= topLeft.y
            && _mousePosition.y <= bottomRight.y;
    }
}
