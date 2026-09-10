#pragma once

#include <cstddef>

#include <CS2/Classes/Vector.h>

namespace cs2 {
    struct AABB_t {
        Vector m_vMinBounds;
        Vector m_vMaxBounds;
    };
}

static_assert(offsetof(cs2::AABB_t, m_vMinBounds) == 0x0);
static_assert(offsetof(cs2::AABB_t, m_vMaxBounds) == 0xC);
static_assert(sizeof(cs2::AABB_t) == 0x18);
