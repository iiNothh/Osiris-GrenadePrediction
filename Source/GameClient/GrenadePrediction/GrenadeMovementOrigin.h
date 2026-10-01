#pragma once

#include <bit>
#include <cstdint>
#include <xmmintrin.h>

#include <CS2/Classes/Vector.h>

struct GrenadeMovementOrigin {
    [[nodiscard]] static bool commit(cs2::Vector& stored, cs2::Vector candidate, float maxCoord) noexcept
    {
        if (!isFinite(candidate.x) || !isFinite(candidate.y) || !isFinite(candidate.z)
            || !isFinite(maxCoord) || maxCoord <= 0.0f)
            return false;

        const auto epsilon = _mm_set_ss(std::bit_cast<float>(std::uint32_t{0x3B800000}));
        const auto relativeEpsilon = _mm_div_ss(epsilon, _mm_set_ss(maxCoord));
        if (!axisNear(stored.x, candidate.x, relativeEpsilon, epsilon)
            || !axisNear(stored.y, candidate.y, relativeEpsilon, epsilon)
            || !axisNear(stored.z, candidate.z, relativeEpsilon, epsilon))
            stored = candidate;
        return true;
    }

private:
    [[nodiscard]] static bool isFinite(float value) noexcept
    {
        return (std::bit_cast<std::uint32_t>(value) & 0x7F800000u) != 0x7F800000u;
    }

    [[nodiscard]] static bool axisNear(float oldValue, float candidate, __m128 relativeEpsilon, __m128 epsilon) noexcept
    {
        const auto oldScalar = _mm_set_ss(oldValue);
        const auto candidateScalar = _mm_set_ss(candidate);
        const auto sign = _mm_set_ss(-0.0f);
        const auto difference = _mm_andnot_ps(sign, _mm_sub_ss(oldScalar, candidateScalar));
        const auto magnitude = _mm_max_ss(_mm_andnot_ps(sign, oldScalar), _mm_andnot_ps(sign, candidateScalar));
        const auto relativeThreshold = _mm_mul_ss(magnitude, relativeEpsilon);
        const auto threshold = _mm_max_ss(epsilon, relativeThreshold);
        return _mm_comige_ss(threshold, difference) != 0;
    }
};
