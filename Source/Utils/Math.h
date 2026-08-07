#pragma once

#include <xmmintrin.h>

namespace Math {
    [[nodiscard]] inline bool isFinite(float x) noexcept
    {
        constexpr float kMaxFinite = 3.402823466e+38F;
        return x == x && x >= -kMaxFinite && x <= kMaxFinite;
    }

    [[nodiscard]] inline float abs(float x) noexcept
    {
        return _mm_cvtss_f32(_mm_andnot_ps(_mm_set_ss(-0.0f), _mm_set_ss(x)));
    }

    inline void sincos(float x, float& sine, float& cosine) noexcept
    {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kHalfPi = kPi * 0.5f;
        constexpr float kTwoPi = kPi * 2.0f;
        constexpr float kInvTwoPi = 1.0f / kTwoPi;
        constexpr float kMinimumConvertibleFloat = -2147483648.0f;
        constexpr float kMaximumConvertibleFloat = 2147483520.0f;

        if (!isFinite(x)) {
            // Trigonometric functions are undefined for non-finite inputs.
            sine = x - x;
            cosine = sine;
            return;
        }

        const auto turns = x * kInvTwoPi;
        if (turns < kMinimumConvertibleFloat || turns > kMaximumConvertibleFloat) {
            // The phase cannot be reduced accurately without an unsafe conversion; use zero phase.
            sine = 0.0f;
            cosine = 1.0f;
            return;
        }

        x -= static_cast<float>(static_cast<int>(turns)) * kTwoPi;
        if (x > kPi)
            x -= kTwoPi;
        else if (x < -kPi)
            x += kTwoPi;

        float cosineSign = 1.0f;
        if (x > kHalfPi) {
            x = kPi - x;
            cosineSign = -1.0f;
        } else if (x < -kHalfPi) {
            x = -kPi - x;
            cosineSign = -1.0f;
        }

        const float x2 = x * x;
        sine = x * (1.0f + x2 * (-1.0f / 6.0f + x2 * (1.0f / 120.0f + x2 * (-1.0f / 5040.0f + x2 * (1.0f / 362880.0f - x2 / 39916800.0f)))));
        cosine = cosineSign * (1.0f + x2 * (-0.5f + x2 * (1.0f / 24.0f + x2 * (-1.0f / 720.0f + x2 * (1.0f / 40320.0f - x2 / 3628800.0f)))));
    }

    [[nodiscard]] inline float sqrt(float x) noexcept
    {
        return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(x)));
    }
}
