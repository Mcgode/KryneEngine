/**
 * @file
 * @author Max Godefroy
 * @date 09/04/2026.
 */

#pragma once

#include <EASTL/string.h>
#include <EASTL/string_view.h>

namespace KryneEngine::Modules::FileSystem
{
    void NormalizePath(eastl::string_view _src, eastl::string& _dst);
}
