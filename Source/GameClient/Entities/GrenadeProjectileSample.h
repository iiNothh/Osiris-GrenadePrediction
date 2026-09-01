#pragma once

#include <cstdint>

#include <CS2/Classes/Vector.h>

struct GrenadeProjectileSample {
    cs2::Vector origin;
    cs2::Vector velocity;
    float worldTime;
    float createTime;
    float worldTickInterval;
    std::int32_t worldTickCount;
};
