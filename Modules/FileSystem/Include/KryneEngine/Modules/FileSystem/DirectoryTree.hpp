/**
 * @file
 * @author Max Godefroy
 * @date 09/04/2026.
 */

#pragma once

#include <KryneEngine/Core/Common/StringHelpers.hpp>

namespace KryneEngine::Modules::FileSystem
{
    /**
     * @brief A directory tree acceleration structure for efficiently mapping specific parent directories from a path.
     *
     * @details
     * The tree itself is stored as a contiguous array of nodes, with each node representing a directory.
     * Each node contains a hash of the directory name, a pointer to user-defined data, and indices to its child nodes.
     *
     * This structure is optimized to provide fast lookup of the most specific directory for a given file path. For
     * optimal performance, keep directory depth to a minimum to reduce lookup steps.
     *
     * @note
     * The tree only supports child paths to the root directory.
     * Non-child directories won't be added to the tree. Requests with non-child paths are still valid but will always
     * return default results.
     */
    class DirectoryTree
    {
    public:
        explicit DirectoryTree(AllocatorInstance _allocator);

        /**
         * @brief Adds a directory to the tree to map paths to.
         *
         * @return `true` if the directory was successfully mapped, `false` otherwise
         *
         * @warning Make sure the path is normalized.
         */
        [[nodiscard]] bool AddDirectory(eastl::string_view _path, void* _userPtr);

        struct SpecificDirectoryResult
        {
            void* m_directoryPtr = nullptr;
            eastl::string_view m_relativePath;
        };

        /**
         * @brief Find the most specific directory for a given file path.
         *
         * @details
         * For instance, with marked directories 'foo' and 'foo/bar', the most specific directory for 'foo/bar/baz.txt'
         * would be 'foo/bar', while the most specific for 'foo/baz.txt' would be 'foo'.
         *
         * Assuming the resulting directory depth is `N`, the performance cost is:
         *   `N` path component hash computations + `N` linear child array lookups
         *
         *  On average, the cost in linear to the most specific directory depth.
         *
         *  @warning Make sure that the path is normalized.
         */
        [[nodiscard]] SpecificDirectoryResult FindMostSpecificDirectory(eastl::string_view _path) const;

    private:
        struct Node
        {
            StringHashBase m_hash;
            void* m_userPtr = nullptr;
            u32 m_firstChild = -1;
            u32 m_childCount = 0;

            bool operator==(const StringViewHash& _hash) const
            {
                return m_hash == _hash;
            }

            bool operator<(const StringViewHash& _hash) const
            {
                return m_hash < _hash;
            }
        };

        eastl::vector<Node> m_nodes;
    };
} // KryneEngine
