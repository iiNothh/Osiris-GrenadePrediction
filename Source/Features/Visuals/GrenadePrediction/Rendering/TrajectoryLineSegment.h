#pragma once

#include <GameClient/WorldToScreen/ClipSpaceCoordinates.h>
#include <Utils/Math.h>

struct TrajectoryLineSegment {
    static constexpr float kNearW = 0.001f;

    float midpointX;
    float midpointY;
    float width;
    float angleDegrees;

    [[nodiscard]] static bool fromClipSpace(ClipSpaceCoordinates first, ClipSpaceCoordinates second, float aspectRatio, TrajectoryLineSegment& result) noexcept
    {
        if (!hasFiniteCoordinates(first, second, aspectRatio))
            return false;

        if (!clipAgainstNearPlane(first, second))
            return false;

        ViewportSegment segment;
        if (!makeViewportSegment(first, second, segment))
            return false;

        if (!clipToViewport(segment))
            return false;

        return buildResult(segment, aspectRatio, result);
    }

private:
    struct ViewportSegment {
        float x0;
        float y0;
        float x1;
        float y1;
    };

    [[nodiscard]] static bool hasFiniteCoordinates(const ClipSpaceCoordinates& first, const ClipSpaceCoordinates& second, float aspectRatio) noexcept
    {
        return Math::isFinite(first.x) && Math::isFinite(first.y) && Math::isFinite(first.z) && Math::isFinite(first.w)
            && Math::isFinite(second.x) && Math::isFinite(second.y) && Math::isFinite(second.z) && Math::isFinite(second.w)
            && Math::isFinite(aspectRatio) && aspectRatio > kNearW;
    }

    [[nodiscard]] static bool clipAgainstNearPlane(ClipSpaceCoordinates& first, ClipSpaceCoordinates& second) noexcept
    {
        if (first.w < kNearW && second.w < kNearW)
            return false;

        if (first.w < kNearW || second.w < kNearW) {
            const float t = (kNearW - first.w) / (second.w - first.w);
            if (!Math::isFinite(t))
                return false;
            const ClipSpaceCoordinates clipped{
                .x = first.x + (second.x - first.x) * t,
                .y = first.y + (second.y - first.y) * t,
                .z = first.z + (second.z - first.z) * t,
                .w = kNearW};
            if (!Math::isFinite(clipped.x) || !Math::isFinite(clipped.y) || !Math::isFinite(clipped.z))
                return false;
            if (first.w < kNearW)
                first = clipped;
            else
                second = clipped;
        }
        return true;
    }

    [[nodiscard]] static bool makeViewportSegment(const ClipSpaceCoordinates& first, const ClipSpaceCoordinates& second, ViewportSegment& result) noexcept
    {
        result.x0 = first.x / first.w * 0.5f + 0.5f;
        result.y0 = 0.5f - first.y / first.w * 0.5f;
        result.x1 = second.x / second.w * 0.5f + 0.5f;
        result.y1 = 0.5f - second.y / second.w * 0.5f;
        return Math::isFinite(result.x0) && Math::isFinite(result.y0) && Math::isFinite(result.x1) && Math::isFinite(result.y1);
    }

    [[nodiscard]] static bool clipToViewport(ViewportSegment& segment) noexcept
    {
        const float dx = segment.x1 - segment.x0;
        const float dy = segment.y1 - segment.y0;
        if (!Math::isFinite(dx) || !Math::isFinite(dy))
            return false;

        float enter = 0.0f;
        float leave = 1.0f;
        if (!clip(-dx, segment.x0, enter, leave)
            || !clip(dx, 1.0f - segment.x0, enter, leave)
            || !clip(-dy, segment.y0, enter, leave)
            || !clip(dy, 1.0f - segment.y0, enter, leave))
            return false;

        const float clippedX0 = segment.x0 + dx * enter;
        const float clippedY0 = segment.y0 + dy * enter;
        const float clippedX1 = segment.x0 + dx * leave;
        const float clippedY1 = segment.y0 + dy * leave;
        if (!Math::isFinite(clippedX0) || !Math::isFinite(clippedY0) || !Math::isFinite(clippedX1) || !Math::isFinite(clippedY1))
            return false;

        segment = {.x0 = clippedX0, .y0 = clippedY0, .x1 = clippedX1, .y1 = clippedY1};
        return true;
    }

    [[nodiscard]] static bool buildResult(const ViewportSegment& segment, float aspectRatio, TrajectoryLineSegment& result) noexcept
    {
        const float dx = segment.x1 - segment.x0;
        const float dy = segment.y1 - segment.y0;
        if (!Math::isFinite(dx) || !Math::isFinite(dy))
            return false;
        const float physicalDy = dy / aspectRatio;
        const float squaredLength = dx * dx + physicalDy * physicalDy;
        if (!Math::isFinite(physicalDy) || !Math::isFinite(squaredLength))
            return false;
        const float length = Math::sqrt(squaredLength);
        if (!Math::isFinite(length) || length <= 0.00001f)
            return false;

        result = {.midpointX = (segment.x0 + segment.x1) * 50.0f, .midpointY = (segment.y0 + segment.y1) * 50.0f,
            .width = length * 100.0f, .angleDegrees = calculateAngleDegrees(physicalDy, dx)};
        return Math::isFinite(result.midpointX) && Math::isFinite(result.midpointY) && Math::isFinite(result.width) && Math::isFinite(result.angleDegrees);
    }

    [[nodiscard]] static float calculateAngleDegrees(float y, float x) noexcept
    {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kHalfPi = kPi * 0.5f;
        constexpr float kRadiansToDegrees = 57.295779513082320876f;
        const float absX = Math::abs(x);
        const float absY = Math::abs(y);
        float angle;
        if (absX >= absY) {
            if (absX == 0.0f)
                return 0.0f;
            angle = atan(y / x);
            if (x < 0.0f)
                angle += y < 0.0f ? -kPi : kPi;
        } else {
            angle = kHalfPi - atan(x / y);
            if (y < 0.0f)
                angle -= kPi;
        }
        return angle * kRadiansToDegrees;
    }

    [[nodiscard]] static float atan(float x) noexcept
    {
        const float x2 = x * x;
        return x * (0.9998660f + x2 * (-0.3302995f + x2 * (0.1801410f + x2 * (-0.0851330f + x2 * 0.0208351f))));
    }

    [[nodiscard]] static bool clip(float p, float q, float& enter, float& leave) noexcept
    {
        if (p == 0.0f)
            return q >= 0.0f;
        const float ratio = q / p;
        if (!Math::isFinite(ratio))
            return false;
        if (p < 0.0f) {
            if (ratio > leave)
                return false;
            if (ratio > enter)
                enter = ratio;
        } else {
            if (ratio < enter)
                return false;
            if (ratio < leave)
                leave = ratio;
        }
        return true;
    }
};
