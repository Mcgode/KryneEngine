/**
 * @file
 * @author Max Godefroy
 * @date 09/09/2026.
 */

#pragma once

namespace KryneEngine
{
    struct VkFeatures
    {
        bool m_geometryShaders = false;
        bool m_tessellationShaders = false;
        bool m_meshShaders = false;

        bool m_rayTracing = false;
    };
}