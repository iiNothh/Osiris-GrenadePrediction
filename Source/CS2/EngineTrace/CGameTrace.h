#pragma once

#include <cstddef>
#include <cstdint>

#include <CS2/Classes/Vector.h>
#include <CS2/EngineTrace/CTraceFilter.h>
#include <CS2/EngineTrace/RnQueryShapeAttr_t.h>
#include <Utils/ByteStorage.h>

namespace cs2::engine_trace {
    constexpr std::size_t kCGameTraceCapacity{0xC0};
    constexpr std::int32_t kWorldEntityHandle{0x8000};
}

namespace cs2 {
    struct IVPhysics2World;

    struct alignas(16) CGameTrace {
        std::byte storage[engine_trace::kCGameTraceCapacity]{};

        template <typename T>
        [[nodiscard]] T readValue(std::size_t offset) const noexcept
        {
            return byte_storage::read<T>(storage, offset).valueOr(T{});
        }
    };

    using PhysicsWorldPointerSlot = IVPhysics2World**;
    using PhysicsWorldPointerSlotStorage = PhysicsWorldPointerSlot*;
    using TraceShapeFunction = bool (*)(PhysicsWorldPointerSlot, const RnQueryShapeAttr_t*, const Vector*, const Vector*, CTraceFilter*, CGameTrace*) noexcept;
}

static_assert(sizeof(cs2::CGameTrace) == 0xC0);
static_assert(alignof(cs2::CGameTrace) == 16);
