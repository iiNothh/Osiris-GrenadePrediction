#pragma once

#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheUpdater.h>

class GrenadePredictionController {
public:
    static bool completeHeldThrow(GrenadePredictionState& state, const void* weapon, bool hasCurtime, float curtime) noexcept
    {
        if (!state.throwObservation.consumeActualExecution(hasCurtime, curtime))
            return false;

        if (state.ownsTempTrajectory(weapon, state.throwObservation.pendingSequence()))
            state.commitTempTrajectory(curtime, hasCurtime);
        state.invalidateTempTrajectory();
        return true;
    }

    template <typename Simulate>
    static bool completeLiveGrenadeScan(GrenadePredictionState& state, cs2::CEntityHandle localPawnHandle, Optional<float> currentTime, Simulate&& simulate) noexcept
    {
        LiveGrenadeCacheUpdater{state.liveGrenadeCache}.endScan();
        state.liveGrenadeAuthority.observeLocalPawn(localPawnHandle);
        state.liveGrenadeAuthority.update(state.liveGrenadeCache, currentTime);

        const auto projectile = state.liveGrenadeAuthority.newestLocalProjectile(state.liveGrenadeCache);
        if (!projectile.hasValue() || !state.liveGrenadeAuthority.shouldAdopt(projectile.value()) || !simulate(projectile.value()))
            return false;

        state.commitLiveTrajectory(currentTime.valueOr(0.0f), currentTime.hasValue());
        state.liveGrenadeAuthority.accept(projectile.value(), currentTime);
        return true;
    }

    template <typename Projectile, typename Decoy>
    static bool updateDecoyLiveGrenade(LiveGrenadeCache& cache, const Projectile& projectile, cs2::CEntityHandle projectileHandle, const Decoy& decoy) noexcept
    {
        return LiveGrenadeCacheUpdater{cache}.update(projectile, projectileHandle, cs2::GrenadeKind::Decoy, {.decoyShotTick = decoy.decoyShotTick()});
    }
};
