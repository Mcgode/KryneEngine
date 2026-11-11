/**
* @file
* @author Max Godefroy
* @date 11/10/2024.
*/

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include <gtest/gtest.h>

#include "Common.h"
#include "Utils/AssertUtils.hpp"
#include "Utils/Comparison.hpp"

namespace KryneEngine::Tests::Graphics
{
    TEST(GraphicsContext, Creation)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        const GraphicsCommon::ApplicationInfo appInfo = DefaultAppInfo();

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            GraphicsCommon::ContextCreationError error = GraphicsCommon::ContextCreationError::ApiNotSupported;
            auto* context = GraphicsContext::Create(appInfo, AllocatorInstance(), &error);
            EXPECT_NE(context, nullptr);
            EXPECT_EQ(error, GraphicsCommon::ContextCreationError::None);
            GraphicsContext::Destroy(context);
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }

    TEST(GraphicsContext, CreationFailure)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        GraphicsCommon::ApplicationInfo appInfo = DefaultAppInfo();

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        // An API the backend doesn't implement fails creation softly, without asserting.
        {
            appInfo.m_api = GraphicsCommon::Api::None;
            GraphicsCommon::ContextCreationError error = GraphicsCommon::ContextCreationError::None;
            GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, AllocatorInstance(), &error);
            EXPECT_EQ(graphicsContext, nullptr);
            EXPECT_EQ(error, GraphicsCommon::ContextCreationError::ApiNotSupported);
            GraphicsContext::Destroy(graphicsContext);
        }

#if defined(KE_GRAPHICS_API_MTL)
        // Metal has no geometry shaders: a forced request fails creation, a soft one doesn't.
        {
            appInfo = DefaultAppInfo();
            appInfo.m_features.m_geometryShader = GraphicsCommon::SoftEnable::ForceEnabled;
            GraphicsCommon::ContextCreationError error = GraphicsCommon::ContextCreationError::None;
            GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, AllocatorInstance(), &error);
            EXPECT_EQ(graphicsContext, nullptr);
            EXPECT_EQ(error, GraphicsCommon::ContextCreationError::GeometryShaderNotSupported);

            appInfo.m_features.m_geometryShader = GraphicsCommon::SoftEnable::TryEnable;
            graphicsContext = GraphicsContext::Create(appInfo, AllocatorInstance(), &error);
            EXPECT_NE(graphicsContext, nullptr);
            EXPECT_EQ(error, GraphicsCommon::ContextCreationError::None);
            GraphicsContext::Destroy(graphicsContext);
        }
#else
        // Geometry shader support depends on the host: a forced request either succeeds, or reports exactly why.
        {
            appInfo = DefaultAppInfo();
            appInfo.m_features.m_geometryShader = GraphicsCommon::SoftEnable::ForceEnabled;
            GraphicsCommon::ContextCreationError error = GraphicsCommon::ContextCreationError::None;
            GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, AllocatorInstance(), &error);
            EXPECT_EQ(
                error,
                graphicsContext != nullptr
                    ? GraphicsCommon::ContextCreationError::None
                    : GraphicsCommon::ContextCreationError::GeometryShaderNotSupported);
            GraphicsContext::Destroy(graphicsContext);
        }
#endif

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }

    TEST(GraphicsContext, GetFrameContextCount)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        GraphicsCommon::ApplicationInfo appInfo = DefaultAppInfo();

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        // Frame context count follows the explicit buffering mode, regardless of swap chain presence.

        {
            appInfo.m_bufferingMode = GraphicsCommon::BufferingMode::Single;
            GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, AllocatorInstance());
            EXPECT_EQ(graphicsContext->GetFrameContextCount(), 1);
            GraphicsContext::Destroy(graphicsContext);
        }

        {
            appInfo.m_bufferingMode = GraphicsCommon::BufferingMode::Double;
            GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, AllocatorInstance());
            EXPECT_EQ(graphicsContext->GetFrameContextCount(), 2);
            GraphicsContext::Destroy(graphicsContext);
        }

        {
            appInfo.m_bufferingMode = GraphicsCommon::BufferingMode::Triple;
            GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, AllocatorInstance());
            EXPECT_EQ(graphicsContext->GetFrameContextCount(), 3);
            GraphicsContext::Destroy(graphicsContext);
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }

    TEST(GraphicsContext, GetApplicationInfo)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        const GraphicsCommon::ApplicationInfo appInfo = DefaultAppInfo();
        GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, AllocatorInstance());

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const GraphicsCommon::ApplicationInfo gAppInfo = graphicsContext->GetApplicationInfo();

        EXPECT_EQ(appInfo.m_applicationName, gAppInfo.m_applicationName);
        EXPECT_BINARY_EQ(appInfo.m_applicationVersion, gAppInfo.m_applicationVersion);

        EXPECT_BINARY_EQ(appInfo.m_engineVersion, gAppInfo.m_engineVersion);
        EXPECT_EQ(appInfo.m_api, appInfo.m_api);


        static_assert(
            std::has_unique_object_representations_v<GraphicsCommon::ApplicationInfo::Features>,
            "ApplicationInfo::Features must stay padding-free — it is byte-compared in the unit tests");
        EXPECT_BINARY_EQ(appInfo.m_features, gAppInfo.m_features);

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        GraphicsContext::Destroy(graphicsContext);

        catcher.ExpectNoMessage();
    }
}
