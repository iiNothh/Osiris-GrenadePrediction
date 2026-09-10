#pragma once

#include <cstdint>

#include <CS2/Classes/Vector.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

struct TraceResult {
    float fraction{};
    cs2::Vector endPos{};
    cs2::Vector normal{};
    Optional<std::int32_t> rawEntityHandle{};

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return Math::isFinite(fraction) && fraction >= 0.0f && fraction <= 1.0f && endPos.isFinite() && normal.isFinite() && (fraction == 1.0f || !normal.isZero());
    }
};
