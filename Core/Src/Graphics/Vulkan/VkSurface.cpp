/**
 * @file
 * @author Max Godefroy
 * @date 21/03/2022.
 */

#include "Graphics/Vulkan/VkSurface.hpp"

#include "Graphics/Vulkan/HelperFunctions.hpp"
#include "Graphics/Vulkan/VkSurfacePlatform.hpp"

namespace KryneEngine
{
    VkSurface::VkSurface(AllocatorInstance _allocator)
    {
        m_capabilities.m_formats.SetAllocator(_allocator);
        m_capabilities.m_presentModes.SetAllocator(_allocator);
    }

    void VkSurface::Init(VkInstance _instance, const NativeWindowHandle& _nativeWindow)
    {
        KE_ZoneScopedFunction("VkSurface::VkSurface");
        m_surface = VkSurfacePlatform::Create(_instance, _nativeWindow);
    }

    VkSurface::~VkSurface()
    {
        KE_ASSERT(!m_surface);
    }

    void VkSurface::Destroy(VkInstance _instance)
    {
        vkDestroySurfaceKHR(_instance, SafeReset(m_surface), nullptr);
    }

    void VkSurface::UpdateCapabilities(const VkPhysicalDevice& _physicalDevice)
    {
        KE_ZoneScopedFunction("VkSurface::UpdateCapabilities");

        VkAssert(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, m_surface, &m_capabilities.m_surfaceCapabilities));

        VkHelperFunctions::VkArrayFetch(m_capabilities.m_formats, vkGetPhysicalDeviceSurfaceFormatsKHR, _physicalDevice, m_surface);
        VkHelperFunctions::VkArrayFetch(m_capabilities.m_presentModes, vkGetPhysicalDeviceSurfacePresentModesKHR, _physicalDevice, m_surface);

        KE_ASSERT(!m_capabilities.m_formats.Empty() && !m_capabilities.m_presentModes.Empty());
    }
}