#pragma once

#include <GameClient/Entities/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeLifecycle.h>

class LiveGrenadeCacheUpdater {
public:
    explicit LiveGrenadeCacheUpdater(LiveGrenadeCache& cache) noexcept
        : cache{cache}
    {
    }

    template <typename Projectile>
    [[nodiscard]] bool update(const Projectile& projectile, cs2::CEntityHandle projectileHandle, GrenadeKind kind,
        const LiveGrenadeLifecycleState& lifecycleState = {}) noexcept
    {
        const auto initialPosition = projectile.initialPosition();
        const auto initialVelocity = projectile.initialVelocity();
        const auto thrower = projectile.thrower();
        if (!initialPosition.hasValue() || !initialVelocity.hasValue() || !thrower.hasValue())
            return false;

        return cache.upsert({
            .projectileHandle = projectileHandle,
            .throwerHandle = thrower.value(),
            .initialPosition = initialPosition.value(),
            .initialVelocity = initialVelocity.value(),
            .kind = kind,
            .lifecycleEnded = getLiveGrenadeLifecycle(kind, lifecycleState) == LiveGrenadeLifecycle::Remove
        });
    }

private:
    LiveGrenadeCache& cache;
};
