#pragma once

#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheUpdater.h>

class GrenadePredictionController {
public:
    static void observeHeldThrow(GrenadeThrowObservation& observation, const void* weapon, bool pinPulled, auto&& readThrowStrength) noexcept
    {
        static_cast<void>(observation.observePinState(weapon, pinPulled));
        if (pinPulled) {
            const auto throwStrength = readThrowStrength();
            if (throwStrength.hasValue())
                observation.retainThrowStrength(throwStrength.value());
        }
    }

    static bool completeHeldThrow(GrenadePredictionState& state, const void* weapon, bool hasCurtime, float curtime) noexcept
    {
        if (!state.throwObservation.consumeActualExecution(hasCurtime, curtime))
            return false;

        const bool stagedTrajectoryReady = state.stageOwnedTempTrajectory(weapon, state.throwObservation.pendingSequence());
        state.finalizeStagedTrajectory(stagedTrajectoryReady, hasCurtime, curtime);
        return true;
    }

    template <typename Simulate>
    static bool completeLiveGrenadeScan(GrenadePredictionState& state, cs2::CEntityHandle localPawnHandle, Optional<float> currentTime, Simulate&& simulate) noexcept
    {
        LiveGrenadeCacheUpdater{state.liveGrenadeCache}.endScan();
        state.liveGrenadeAuthority.observeLocalPawn(localPawnHandle);
        state.liveGrenadeAuthority.update(state.liveGrenadeCache, currentTime);

        const auto projectile = state.liveGrenadeAuthority.newestLocalProjectile(state.liveGrenadeCache);
        if (!projectile.hasValue() || !state.liveGrenadeAuthority.observeForSimulation(projectile.value()) || !simulate(projectile.value()))
            return false;

        state.commitLiveGrenadeTrajectory();
        state.finalizeStagedTrajectory(true, currentTime.hasValue(), currentTime.valueOr(0.0f));
        state.liveGrenadeAuthority.accept(projectile.value(), currentTime);
        return true;
    }

    template <typename Projectile, typename Decoy>
    static bool updateDecoyLiveGrenade(LiveGrenadeCache& cache, const Projectile& projectile, cs2::CEntityHandle projectileHandle, const Decoy& decoy) noexcept
    {
        return LiveGrenadeCacheUpdater{cache}.update(projectile, projectileHandle, cs2::GrenadeKind::Decoy, {.decoyShotTick = decoy.decoyShotTick()});
    }

    template <typename Projectile>
    static bool updateHELiveGrenade(LiveGrenadeCache& cache, const Projectile& projectile, cs2::CEntityHandle projectileHandle) noexcept
    {
        return LiveGrenadeCacheUpdater{cache}.update(projectile, projectileHandle, cs2::GrenadeKind::HEGrenade,
            {.heExplodeEffectTickBegin = projectile.explodeEffectTickBegin()});
    }
};
