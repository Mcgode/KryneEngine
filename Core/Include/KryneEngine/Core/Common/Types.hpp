/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2022.
 */

#pragma once

#include <cstdint>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <glm/detail/type_vec1.hpp>
#include <glm/detail/type_vec2.hpp>
#include <glm/detail/type_vec3.hpp>
#include <glm/detail/type_vec4.hpp>

namespace KryneEngine
{
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCInconsistentNamingInspection"
    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;

    using s8 = int8_t;
    using s16 = int16_t;
    using s32 = int32_t;
    using s64 = int64_t;
#pragma clang diagnostic pop

    struct Size16x2
    {
        u16 m_width = 0;
        u16 m_height = 0;
    };

    struct Rect
    {
        u32 m_left;
        u32 m_top;
        u32 m_right;
        u32 m_bottom;
    };

#if __cpp_if_consteval >= 202106L
#   define IF_CONSTEVAL if consteval
#else
#   define IF_CONSTEVAL if (std::is_constant_evaluated())
#endif
}