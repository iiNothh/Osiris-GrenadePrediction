#pragma once

#include <cstdint>

#include <CS2/Classes/EntitySystem/CEntityHandle.h>
#include <CS2/Classes/Vector.h>
#include <GameClient/Entities/GrenadeKind.h>

struct HeldGrenadeSimulationInput {
    cs2::Vector launchOrigin{};
    cs2::Vector launchVelocity{};
    GrenadeKind kind{GrenadeKind::None};
    float gravity{};
    std::uint64_t collisionSnapshotRevision{};
    cs2::CEntityHandle localPawnHandle{};
    const void* weapon{};
    std::uint32_t throwSequence{};

    [[nodiscard]] bool exactlyEquals(const HeldGrenadeSimulationInput& other) const noexcept
    {
        return launchOrigin.x == other.launchOrigin.x && launchOrigin.y == other.launchOrigin.y && launchOrigin.z == other.launchOrigin.z
            && launchVelocity.x == other.launchVelocity.x && launchVelocity.y == other.launchVelocity.y && launchVelocity.z == other.launchVelocity.z
            && kind == other.kind && gravity == other.gravity && collisionSnapshotRevision == other.collisionSnapshotRevision
            && localPawnHandle == other.localPawnHandle && weapon == other.weapon && throwSequence == other.throwSequence;
    }
};
