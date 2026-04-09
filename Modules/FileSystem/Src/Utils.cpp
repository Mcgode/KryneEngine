/**
 * @file
 * @author Max Godefroy
 * @date 09/04/2026.
 */

#include "KryneEngine/Modules/FileSystem/Utils.hpp"

#include <KryneEngine/Core/Common/Assert.hpp>

namespace KryneEngine::Modules::FileSystem
{
    void NormalizePath(const eastl::string_view _src, eastl::string& _dst)
    {
        _dst.clear();
        _dst.reserve(_src.size() + 1);

        size_t srcIndex = 0;
        bool firstChar = true;
        while (srcIndex < _src.size())
        {
            if (_src[srcIndex] == '\\' || _src[srcIndex] == '/')
            {
                if (!_dst.empty() && _dst.back() != '/')
                    _dst.push_back('/');
                firstChar = true;
                ++srcIndex;
                continue;
            }

            if (_src[srcIndex] == '.' && firstChar)
            {
                if (srcIndex + 1 >= _src.size())
                {
                    break;
                }

                if (_src[srcIndex + 1] == '\\' || _src[srcIndex + 1] == '/')
                {
                    ++srcIndex;
                    continue;
                }
                if (_src[srcIndex + 1] == '.')
                {
                    if (srcIndex + 2 >= _src.size() || (_src[srcIndex + 2] == '\\' || _src[srcIndex + 2] == '/'))
                    {
                        // If dst is empty, we are at relative root, simply add '..'
                        // If not empty, we might already have other prefixing '..', if that's the case, add another
                        // '..' level
                        if (_dst.empty() || _dst.size() > 3 && (_dst[_dst.size() - 3] == '.' && _dst[_dst.size() - 2] == '.'))
                        {
                            _dst.push_back('.');
                            _dst.push_back('.');
                            srcIndex += 2;
                            continue;
                        }
                        // If neither of the two options, it means we are going back from the last dir, so remove it.
                        KE_ASSERT(_dst.back() == '/');
                        _dst.pop_back();
                        while (!_dst.empty() && _dst.back() != '/')
                            _dst.pop_back();
                        srcIndex += 2;
                        continue;
                    }
                }
            }

            firstChar = false;
            _dst.push_back(_src[srcIndex]);
            ++srcIndex;
        }

        if (!_dst.empty())
        {
            if (_dst.back() == '/')
                _dst.pop_back();
            if (_dst.back() != '\0')
                _dst.push_back('\0');
        }

    }
}
