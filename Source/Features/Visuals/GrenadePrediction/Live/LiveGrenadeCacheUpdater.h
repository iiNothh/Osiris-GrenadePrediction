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

    void beginScan() noexcept
    {
        cache.beginScan();
    }

    void endScan() noexcept
    {
        cache.endScan();
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
            .lifecycleEnded = getLiveGrenadeLifecycle(kind, lifecycleState) == LiveGrenadeLifecycle::Remove,
            .currentSample = currentSample(projectile)
        });
    }

private:
    template <typename Projectile>
    [[nodiscard]] static Optional<GrenadeProjectileSample> currentSample(const Projectile& projectile) noexcept
    {
        if constexpr (requires { projectile.currentSample(); })
            return projectile.currentSample();
        else return {};
    }

    LiveGrenadeCache& cache;
};
