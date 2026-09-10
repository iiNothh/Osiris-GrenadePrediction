#pragma once

#include <cstdint>

namespace cs2
{
    enum class PhysicsQueryFlag : std::uint8_t {
        None = 0,
        IncludeSolidContacts = 1 << 0,
        RespectDisabledSolidContacts = 1 << 1,
        IncludeTriggerContacts = 1 << 2,
        RespectIgnoredPairs = 1 << 3,
        ExcludePickupSolids = 1 << 4,
        ForceCollision = 1 << 5
    };

    [[nodiscard]] constexpr PhysicsQueryFlag operator|(PhysicsQueryFlag lhs, PhysicsQueryFlag rhs) noexcept
    {
        return static_cast<PhysicsQueryFlag>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
    }
}
