#pragma once

#include <cstddef>
#include <CS2/Constants/CollisionGroup.h>
#include <CS2/Constants/InteractionLayers.h>
#include <CS2/Constants/PhysicsQueryFlag.h>
#include <Utils/ByteStorage.h>

namespace cs2::engine_trace {
    constexpr std::size_t kCTraceFilterCapacity{0x48};
}

namespace cs2 {
    struct alignas(8) CTraceFilter {
        std::byte storage[engine_trace::kCTraceFilterCapacity]{};

        template <typename T>
        [[nodiscard]] T readValue(std::size_t offset) const noexcept
        {
            return byte_storage::read<T>(storage, offset).valueOr(T{});
        }

        template <typename T>
        void writeValue(std::size_t offset, T value) noexcept
        {
            static_cast<void>(byte_storage::write<T>(storage, offset, value));
        }
    };

    using CTraceFilterConstructionFunction = CTraceFilter* (*)(CTraceFilter*, void*, engine_trace::InteractionLayer interactsWith, CollisionGroup, PhysicsQueryFlag) noexcept;
    using CTraceFilterAddExcludedEntityFunction = void (*)(CTraceFilter*, void*, void*) noexcept;
}

namespace cs2::engine_trace {
    static_assert(sizeof(cs2::CTraceFilter) == 0x48 && alignof(cs2::CTraceFilter) == 8);
}
