/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2025.
 */

#pragma once

#include "KryneEngine/Core/Math/Vector.hpp"

#ifndef KE_DEFAULT_COORDINATE_SYSTEM
#   define KE_DEFAULT_COORDINATE_SYSTEM KryneEngine::Math::CoordinateSystem::RightHandedZUp
#endif

#ifndef KE_DEFAULT_EULER_ORDER
#   define KE_DEFAULT_EULER_ORDER KryneEngine::Math::EulerOrder::ZXY // Yaw -> pitch -> roll in RightHandedZUp
#endif

namespace KryneEngine::Math
{
    /**
     * @brief Represents different types of coordinate systems often used in graphics and computational geometry.
     *
     * @details
     * Each value specifies the handedness (left-handed or right-handed) and the axis that points
     * upwards (Y-up or Z-up) in a given coordinate system.
     * The X axis is always assumed to point to the right.
     *
     * Definitions of each enumeration constant:
     * - LeftHandedYUp: Defines a left-handed coordinate system with the Y-axis pointing upwards.
     * - LeftHandedZUp: Defines a left-handed coordinate system with the Z-axis pointing upwards.
     * - RightHandedYUp: Defines a right-handed coordinate system with the Y-axis pointing upwards.
     * - RightHandedZUp: Defines a right-handed coordinate system with the Z-axis pointing upwards.
     */
    enum class CoordinateSystem
    {
        LeftHandedYUp,
        LeftHandedZUp,
        RightHandedYUp,
        RightHandedZUp,
    };

    constexpr bool IsLeftHanded(const CoordinateSystem _system)
    {
        return _system == CoordinateSystem::LeftHandedYUp || _system == CoordinateSystem::LeftHandedZUp;
    }

    constexpr bool IsZUp(const CoordinateSystem _system)
    {
        return _system == CoordinateSystem::RightHandedZUp || _system == CoordinateSystem::LeftHandedZUp;
    }

    inline float3 UpVector(const CoordinateSystem _system = KE_DEFAULT_COORDINATE_SYSTEM)
    {
        return IsZUp(_system) ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    }

    inline float3 RightVector(const CoordinateSystem _system = KE_DEFAULT_COORDINATE_SYSTEM)
    {
        return float3 { 1.0f, 0.0f, 0.0f };
    }

    inline float3 ForwardVector(const CoordinateSystem _system = KE_DEFAULT_COORDINATE_SYSTEM)
    {
        switch (_system)
        {
        case CoordinateSystem::LeftHandedYUp:
            return float3 { 0.0f, 0.0f, 1.0f };
        case CoordinateSystem::RightHandedYUp:
            return float3 { 0.0f, 0.0f, -1.0f };
        case CoordinateSystem::LeftHandedZUp:
            return float3 { 0.0f, -1.0f, 0.0f };
        case CoordinateSystem::RightHandedZUp:
        default:
            return float3 { 0.0f, 1.0f, 0.0f };
        };
    }

    enum class EulerOrder
    {
        XYZ,
        XZY,
        YXZ,
        YZX,
        ZXY,
        ZYX
    };

    static constexpr CoordinateSystem kDefaultCoordinateSystem = KE_DEFAULT_COORDINATE_SYSTEM;
    static constexpr EulerOrder kDefaultEulerOrder = KE_DEFAULT_EULER_ORDER;
}