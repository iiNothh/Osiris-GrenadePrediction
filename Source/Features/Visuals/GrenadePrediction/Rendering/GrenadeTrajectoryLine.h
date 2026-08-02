#pragma once

#include <cstddef>

#include <CS2/Classes/Vector.h>

namespace grenade_prediction
{

namespace grenade_trajectory_line_detail
{

[[nodiscard]] inline float squareRoot(float value) noexcept
{
    if (value <= 0.0f)
        return 0.0f;
    float result = value > 1.0f ? value : 1.0f;
    for (std::size_t i = 0; i < 8; ++i)
        result = (result + value / result) * 0.5f;
    return result;
}

[[nodiscard]] inline float arctangent(float value) noexcept
{
    constexpr auto kHalfPi{1.5707963f};
    const auto absoluteValue = value < 0.0f ? -value : value;
    const auto reducedValue = absoluteValue > 1.0f ? 1.0f / absoluteValue : absoluteValue;
    const auto squared = reducedValue * reducedValue;
    const auto result = reducedValue * (0.9998546f + squared * (-0.3302995f + squared * (0.1801410f + squared * (-0.0851330f + squared * 0.0208351f))));
    const auto angle = absoluteValue > 1.0f ? kHalfPi - result : result;
    return value < 0.0f ? -angle : angle;
}

[[nodiscard]] inline float angle(float y, float x) noexcept
{
    constexpr auto kPi{3.14159265f};
    constexpr auto kHalfPi{1.5707963f};
    if (x == 0.0f) {
        if (y == 0.0f)
            return 0.0f;
        return y >= 0.0f ? kHalfPi : -kHalfPi;
    }
    if (x > 0.0f)
        return arctangent(y / x);
    return y >= 0.0f ? arctangent(y / x) + kPi : arctangent(y / x) - kPi;
}

}

struct GrenadeProjectedPoint {
    float x{};
    float y{};
    float depth{};
};

struct GrenadeTrajectoryLine {
    GrenadeProjectedPoint start{};
    GrenadeProjectedPoint end{};

    [[nodiscard]] float lengthSquared() const noexcept
    {
        const auto dx = end.x - start.x;
        const auto dy = end.y - start.y;
        return dx * dx + dy * dy;
    }

    [[nodiscard]] float length() const noexcept
    {
        return grenade_trajectory_line_detail::squareRoot(lengthSquared());
    }

    [[nodiscard]] float angleDegrees() const noexcept
    {
        return grenade_trajectory_line_detail::angle(end.y - start.y, end.x - start.x) * 57.2957795f;
    }
};

struct GrenadeTrajectoryLineClipper {
    [[nodiscard]] static bool project(const cs2::Vector& start, const cs2::Vector& end, auto&& toClipSpace, GrenadeTrajectoryLine& line) noexcept
    {
        const auto first = toClipSpace(start);
        const auto second = toClipSpace(end);
        if (first.w <= 0.001f && second.w <= 0.001f)
            return false;

        GrenadeProjectedPoint a{first.x / first.w, first.y / first.w, first.z / first.w};
        GrenadeProjectedPoint b{second.x / second.w, second.y / second.w, second.z / second.w};
        if (!clip(a, b))
            return false;
        line = GrenadeTrajectoryLine{a, b};
        return true;
    }

private:
    [[nodiscard]] static bool clip(GrenadeProjectedPoint& a, GrenadeProjectedPoint& b) noexcept
    {
        const auto dx = b.x - a.x;
        const auto dy = b.y - a.y;
        float first = 0.0f;
        float last = 1.0f;
        if (!clipAxis(-dx, a.x + 1.0f, first, last) || !clipAxis(dx, 1.0f - a.x, first, last)
            || !clipAxis(-dy, a.y + 1.0f, first, last) || !clipAxis(dy, 1.0f - a.y, first, last))
            return false;
        const auto original = a;
        a.x = original.x + dx * first;
        a.y = original.y + dy * first;
        a.depth = original.depth + (b.depth - original.depth) * first;
        b.x = original.x + dx * last;
        b.y = original.y + dy * last;
        b.depth = original.depth + (b.depth - original.depth) * last;
        return true;
    }

    [[nodiscard]] static bool clipAxis(float p, float q, float& first, float& last) noexcept
    {
        if (p == 0.0f)
            return q >= 0.0f;
        const auto ratio = q / p;
        if (p < 0.0f) {
            if (ratio > last)
                return false;
            if (ratio > first)
                first = ratio;
        } else {
            if (ratio < first)
                return false;
            if (ratio < last)
                last = ratio;
        }
        return true;
    }
};

}
