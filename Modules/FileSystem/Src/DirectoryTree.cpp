/**
 * @file
 * @author Max Godefroy
 * @date 09/04/2026.
 */

#include "KryneEngine/Modules/FileSystem/DirectoryTree.hpp"

namespace KryneEngine::Modules::FileSystem
{
    DirectoryTree::DirectoryTree(const AllocatorInstance _allocator)
        : m_nodes(_allocator)
    {
        m_nodes.push_back({
            .m_hash = StringHashBase(Hashing::Hash64(eastl::string_view(""))),
        });
    }

    bool DirectoryTree::AddDirectory(const eastl::string_view _path, void* _userPtr)
    {
        if (_path.size() >= 2 && _path[0] == '.' && _path[1] == '.')
        {
            return false;
        }

        size_t offset = 0;
        size_t currentNodeIdx = 0;
        while (offset < _path.size())
        {
            size_t strEnd = _path.find_first_of('/', offset);
            if (strEnd == eastl::string_view::npos)
                strEnd = _path.size();
            const StringViewHash directoryHash { _path.substr(offset, strEnd - offset) };

            size_t nextNodeIdx;
            Node* currentNode = &m_nodes[currentNodeIdx];
            if (currentNode->m_childCount > 0)
            {
                Node* begin = m_nodes.begin() + currentNode->m_firstChild;
                Node* end = m_nodes.begin() + currentNode->m_firstChild + currentNode->m_childCount;
                nextNodeIdx = eastl::find(begin, end, directoryHash) - m_nodes.begin();
            }
            else
            {
                nextNodeIdx = m_nodes.size();
            }

            if (nextNodeIdx == m_nodes.size() || m_nodes[nextNodeIdx].m_hash != directoryHash)
            {
                m_nodes.insert(m_nodes.begin() + nextNodeIdx, Node {
                        .m_hash = StringHashBase(directoryHash.m_hash),
                    });

                for (Node& node : m_nodes)
                {
                    if (node.m_childCount > 0 && node.m_firstChild >= nextNodeIdx)
                        ++node.m_firstChild;
                }

                currentNode = &m_nodes[currentNodeIdx];
                ++currentNode->m_childCount;
                if (currentNode->m_firstChild == -1)
                    currentNode->m_firstChild = nextNodeIdx;
            }
            currentNodeIdx = nextNodeIdx;
            offset = strEnd + 1;
        }
        if (m_nodes[currentNodeIdx].m_userPtr != nullptr)
            return false;
        m_nodes[currentNodeIdx].m_userPtr = _userPtr;
        return true;
    }

    DirectoryTree::SpecificDirectoryResult DirectoryTree::FindMostSpecificDirectory(const eastl::string_view _path) const
    {
        size_t lastDirectoryPathStart = 0;
        void* lastDirectoryUserPtr = nullptr;

        if (_path.size() >= 2 && _path[0] == '.' && _path[1] == '.')
        {
            return {
                lastDirectoryUserPtr,
                { _path.begin() + lastDirectoryPathStart, _path.size() - lastDirectoryPathStart }
            };
        }


        const Node* currentNode = m_nodes.begin();
        if (currentNode->m_userPtr != nullptr)
            lastDirectoryUserPtr = currentNode->m_userPtr;
        while (lastDirectoryPathStart < _path.size())
        {
            if (currentNode->m_childCount == 0)
                break;

            size_t strEnd = _path.find_first_of('/', lastDirectoryPathStart);
            if (strEnd == eastl::string_view::npos)
                strEnd = _path.size();
            const eastl::string_view directoryName { _path.begin() + lastDirectoryPathStart, strEnd - lastDirectoryPathStart };
            const StringViewHash directoryHash { directoryName };

            const Node* begin = m_nodes.begin() + currentNode->m_firstChild;
            const Node* end = m_nodes.begin() + currentNode->m_firstChild + currentNode->m_childCount;
            const Node* it = eastl::find(begin, end, directoryHash);

            if (it == end)
            {
                break;
            }
            else
            {
                currentNode = it;
                lastDirectoryPathStart = strEnd + 1;

                if (currentNode->m_userPtr != nullptr)
                    lastDirectoryUserPtr = currentNode->m_userPtr;
            }
        }

        return {
            lastDirectoryUserPtr,
            lastDirectoryPathStart < _path.size()
             ? _path.substr(lastDirectoryPathStart)
             : eastl::string_view()
        };
    }
} // KryneEngine