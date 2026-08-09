#pragma once

#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>

class LiveGrenadeCacheUpdater {
public:
    explicit LiveGrenadeCacheUpdater(LiveGrenadeCache& cache) noexcept
        : cache{cache}
    {
    }

    void beginScan() noexcept
    {
        cache.beginScan();
    }

    void endScan() noexcept
    {
        cache.endScan();
    }

    template <typename Projectile>
    [[nodiscard]] bool update(const Projectile& projectile, cs2::CEntityHandle projectileHandle, cs2::GrenadeKind kind) noexcept
    {
        const auto initialPosition = projectile.initialPosition();
        const auto initialVelocity = projectile.initialVelocity();
        const auto thrower = projectile.thrower();
        if (!initialPosition.hasValue() || !initialVelocity.hasValue() || !thrower.hasValue())
            return false;

        return cache.upsert({projectileHandle, thrower.value(), initialPosition.value(), initialVelocity.value(), kind});
    }

private:
    LiveGrenadeCache& cache;
};
