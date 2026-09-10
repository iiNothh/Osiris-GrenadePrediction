#pragma once

#include <cstddef>

#include <CS2/EngineTrace/AABB_t.h>

namespace cs2 {
    struct alignas(8) RnQueryShapeAttr_t {
        std::byte storage[0x30];
    };

    using BuildRnQueryShapeAttrFromAABBFunction = void (*)(RnQueryShapeAttr_t* output, const AABB_t* bounds) noexcept;
}

static_assert(sizeof(cs2::RnQueryShapeAttr_t) == 0x30);
static_assert(alignof(cs2::RnQueryShapeAttr_t) == 8);
