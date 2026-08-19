/**
 * @file
 * @author Max Godefroy
 * @date 19/08/2026.
 */

#include "KryneEngine/Core/Math/Quaternion.hpp"

#include <KryneEngine/Core/Math/Vector.hpp>


namespace KryneEngine::Math
{
    template <class T> requires(std::is_floating_point_v<T>)
    QuaternionBase<T> QuaternionBase<T>::operator*(const QuaternionBase& _other) const
    {
        return QuaternionBase(
            w * _other.w - x * _other.x - y * _other.y - z * _other.z,
            w * _other.x + x * _other.w + y * _other.z - z * _other.y,
            w * _other.y - x * _other.z + y * _other.w + z * _other.x,
            w * _other.z + x * _other.y - y * _other.x + z * _other.w);
    }

    template <class T> requires(std::is_floating_point_v<T>)
    bool QuaternionBase<T>::operator==(const QuaternionBase& _other) const
    {
        return std::abs(w - _other.w) < kQuaternionEpsilon && std::abs(x - _other.x) < kQuaternionEpsilon
               && std::abs(y - _other.y) < kQuaternionEpsilon && std::abs(z - _other.z) < kQuaternionEpsilon;
    }

    template <class T> requires(std::is_floating_point_v<T>)
    T QuaternionBase<T>::Length2() const
    {
        return w * w + x * x + y * y + z * z;
    }

    template <class T> requires(std::is_floating_point_v<T>)
    T QuaternionBase<T>::Length() const
    {
        return std::sqrt(Length2());
    }

    template <class T> requires(std::is_floating_point_v<T>)
    QuaternionBase<T>& QuaternionBase<T>::Normalize()
    {
        const T length = Length();
        w /= length;
        x /= length;
        y /= length;
        z /= length;
        return *this;
    }

    template <class T> requires(std::is_floating_point_v<T>)
    QuaternionBase<T>& QuaternionBase<T>::Conjugate()
    {
        x = -x;
        y = -y;
        z = -z;
        return *this;
    }

    template <class T> requires(std::is_floating_point_v<T>)
    QuaternionBase<T>& QuaternionBase<T>::Inverse()
    {
        return Conjugate();
    }

    template <class T> requires(std::is_floating_point_v<T>)
    T QuaternionBase<T>::Dot(const QuaternionBase& _a, const QuaternionBase& _b)
    {
        return _a.w * _b.w + _a.x * _b.x + _a.y * _b.y + _a.z * _b.z;
    }

    template <class T> requires(std::is_floating_point_v<T>)
    QuaternionBase<T>& QuaternionBase<T>::Slerp(const QuaternionBase& _other, T _t)
    {
        if (_t == 0)
        {
            return *this;
        }
        else if (_t == 1)
        {
            *this = _other;
            return *this;
        }

        T cosHalfTheta = Dot(*this, _other);

        // If _a == _b or _a = -_b, then theta = 0, we can return a
        if (std::abs(cosHalfTheta) >= 1.0f)
        {
            return *this;
        }

        if (cosHalfTheta < 0)
        {
            w = -w;
            x = -x;
            y = -y;
            z = -z;
            cosHalfTheta = -cosHalfTheta;
        }

        const T halfTheta = std::acos(cosHalfTheta);
        const T sinHalfTheta = std::sqrt(1.0f - cosHalfTheta * cosHalfTheta);

        // If theta = 180°, result is not clearly defined, as we could rotate around any angle
        if (std::abs(sinHalfTheta) < kQuaternionEpsilon)
        {
            w = 0.5 * w + 0.5 * _other.w;
            x = 0.5 * x + 0.5 * _other.x;
            y = 0.5 * y + 0.5 * _other.y;
            z = 0.5 * z + 0.5 * _other.z;
            return *this;
        }

        const T ratioA = std::sin((1.0f - _t) * halfTheta) / sinHalfTheta;
        const T ratioB = std::sin(_t * halfTheta) / sinHalfTheta;
        w = ratioA * w + ratioB * _other.w;
        x = ratioA * x + ratioB * _other.x;
        y = ratioA * y + ratioB * _other.y;
        z = ratioA * z + ratioB * _other.z;
        return *this;
    }


    template <class T>
    uint4 PackLowestThree(const QuaternionBase<T>& _q, u32 _bits)
    {
        u8 maxIdx = 0;
        {
            T maxVal = std::abs(_q.w);
            if (maxVal < std::abs(_q.x)) { maxIdx = 1; maxVal = std::abs(_q.x); }
            if (maxVal < std::abs(_q.y)) { maxIdx = 2; maxVal = std::abs(_q.y); }
            if (maxVal < std::abs(_q.z)) { maxIdx = 3; maxVal = std::abs(_q.z); }
        }

        T sign = 1.;
        if (_q.Ptr()[maxIdx] < 0) { sign = -1.; }

        T a, b, c;
        switch (maxIdx)
        {
        case 0: a = _q.x; b = _q.y; c = _q.z; break;
        case 1: a = _q.w; b = _q.y; c = _q.z; break;
        case 2: a = _q.w; b = _q.x; c = _q.z; break;
        case 3: a = _q.w; b = _q.x; c = _q.y; break;
        default: break;
        }

        constexpr T halfSqrt = M_SQRT2 * 0.5;
        const T scale = static_cast<T>((1 << (_bits - 1)) - 1) / halfSqrt;

        return {
            maxIdx,
            std::round(a * scale * sign),
            std::round(b * scale * sign),
            std::round(c * scale * sign)
        };
    }

    template <class T>requires(std::is_floating_point_v<T>)
    u32 QuaternionBase<T>::Pack32() const
    {
        const uint4 packValues = PackLowestThree(*this, 10);
        return packValues.x
            | (packValues.y << 02)
            | (packValues.z << 12)
            | (packValues.w << 22);
    }

    template <class T> requires(std::is_floating_point_v<T>)
    u64 QuaternionBase<T>::Pack64() const
    {
        const uint4 packValues = PackLowestThree(*this, 20);
        u64 result = packValues.x;
        result = BitUtils::BitfieldInsert<u64>(result, packValues.y, 20, 2);
        result = BitUtils::BitfieldInsert<u64>(result, packValues.z, 20, 22);
        result = BitUtils::BitfieldInsert<u64>(result, packValues.w, 20, 42);
        return result;
    }

    template struct QuaternionBase<float>;
    template struct QuaternionBase<double>;
} // namespace KryneEngine::Math