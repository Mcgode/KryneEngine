/**
 * @file
 * @author Max Godefroy
 * @date 07/04/2026.
 */

#pragma once

#include <KryneEngine/Core/Common/BitUtils.hpp>

namespace KryneEngine::Modules::FileSystem
{
    enum class FileFlags
    {
        None = 0,
        ZstdCompressed = 1 << 0,
    };

    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(FileFlags)
}
