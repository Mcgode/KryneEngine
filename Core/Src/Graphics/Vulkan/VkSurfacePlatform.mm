/**
 * @file
 * @author Max Godefroy
 * @date 06/09/2026.
 */

#if defined(__APPLE__)

#include "Graphics/Vulkan/VkSurfacePlatform.hpp"

#include <AppKit/AppKit.h>
#include <QuartzCore/CAMetalLayer.h>

#include "Graphics/Vulkan/HelperFunctions.hpp"
#include "KryneEngine/Core/Common/Assert.hpp"

namespace KryneEngine::VkSurfacePlatform
{
    VkSurfaceKHR Create(const VkInstance _instance, const NativeWindowHandle& _nativeWindow)
    {
        KE_ASSERT(_nativeWindow.m_kind == NativeWindowHandle::Kind::Cocoa);

        NSWindow* nsWindow = (__bridge NSWindow*)_nativeWindow.m_windowHandle;
        NSView* contentView = nsWindow.contentView;

        // MoltenVK requires a CAMetalLayer-backed view. Attach one if the view isn't already
        // layer-backed by a metal layer (GLFW leaves this to the client for GLFW_NO_API windows).
        CAMetalLayer* metalLayer;
        if ([contentView.layer isKindOfClass:[CAMetalLayer class]])
        {
            metalLayer = (CAMetalLayer*)contentView.layer;
        }
        else
        {
            metalLayer = [CAMetalLayer layer];
            metalLayer.contentsScale = nsWindow.backingScaleFactor;
            contentView.layer = metalLayer;
            contentView.wantsLayer = YES;
        }

        const VkMetalSurfaceCreateInfoEXT createInfo {
            .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
            .pLayer = metalLayer,
        };

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkAssert(vkCreateMetalSurfaceEXT(_instance, &createInfo, nullptr, &surface));
        return surface;
    }
}

#endif // defined(__APPLE__)
