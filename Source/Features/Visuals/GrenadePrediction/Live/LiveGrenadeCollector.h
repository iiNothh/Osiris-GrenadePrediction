#pragma once

#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeSnapshot.h>
#include <Utils/Optional.h>

namespace grenade_prediction
{

class LiveGrenadeCollector {
public:
    template <typename Projectile>
    [[nodiscard]] static Optional<LiveGrenadeSnapshot> collect(const Projectile& projectile, cs2::CEntityHandle handle, GrenadeKind kind) noexcept
    {
        const auto position = projectile.initialPosition();
        const auto velocity = projectile.initialVelocity();
        const auto thrower = projectile.thrower();
        if (!position.hasValue() || !velocity.hasValue() || !thrower.hasValue() || kind == GrenadeKind::None)
            return {};
        return LiveGrenadeSnapshot{handle, thrower.value(), position.value(), velocity.value(), kind};
    }
};

}
