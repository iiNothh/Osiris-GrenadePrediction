#pragma once

#include <Utils/Math.h>

namespace cs2
{

struct Vector {
    float x, y, z;

    [[nodiscard]] constexpr bool isFinite() const noexcept
    {
        return Math::isFinite(x) && Math::isFinite(y) && Math::isFinite(z);
    }

    [[nodiscard]] constexpr bool isZero() const noexcept
    {
        return x == 0.0f && y == 0.0f && z == 0.0f;
    }

    [[nodiscard]] constexpr bool isLessThanOrEqualTo(const Vector& other) const noexcept
    {
        return x <= other.x && y <= other.y && z <= other.z;
    }

    [[nodiscard]] constexpr float squareDistTo(const Vector& other) const noexcept
    {
        return (*this - other).squareLength();
    }

    [[nodiscard]] constexpr float squareLength() const noexcept
    {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] friend constexpr Vector operator-(const Vector& a, const Vector& b) noexcept
    {
        return Vector{a.x - b.x, a.y - b.y, a.z - b.z};
    }

    [[nodiscard]] friend constexpr Vector operator+(const Vector& a, const Vector& b) noexcept
    {
        return Vector{a.x + b.x, a.y + b.y, a.z + b.z};
    }

    [[nodiscard]] friend constexpr Vector operator*(const Vector& v, float s) noexcept
    {
        return Vector{v.x * s, v.y * s, v.z * s};
    }

    [[nodiscard]] constexpr float dot(const Vector& other) const noexcept
    {
        return x * other.x + y * other.y + z * other.z;
    }

    [[nodiscard]] bool operator==(const Vector&) const = default;
};

}
