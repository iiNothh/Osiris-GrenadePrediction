#pragma once

#include <cstddef>

#include <CS2/Classes/Vector.h>

namespace grenade_prediction
{

struct GrenadeTrajectory {
    static constexpr std::size_t kPointCapacity{500};
    static constexpr std::size_t kBounceCapacity{20};

    cs2::Vector points[kPointCapacity]{};
    cs2::Vector bounces[kBounceCapacity]{};
    std::size_t pointCount{};
    std::size_t bounceCount{};
    cs2::Vector endPosition{};
    bool valid{};
    bool validLanding{true};

    void reset(cs2::Vector startPosition) noexcept
    {
        pointCount = 0;
        bounceCount = 0;
        endPosition = startPosition;
        valid = false;
        validLanding = true;
    }

    void appendPoint(cs2::Vector point) noexcept
    {
        if (pointCount < kPointCapacity)
            points[pointCount++] = point;
    }

    void appendBounce(cs2::Vector bounce) noexcept
    {
        if (bounceCount < kBounceCapacity)
            bounces[bounceCount++] = bounce;
    }
};

}
