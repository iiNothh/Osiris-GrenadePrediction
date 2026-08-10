#pragma once

#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheUpdater.h>

class GrenadePredictionController {
public:
    static bool advanceScheduler(GrenadePredictionUpdateScheduler& scheduler, bool force, Optional<float> frametime) noexcept
    {
        return scheduler.shouldUpdate(force, frametime.hasValue(), frametime.valueOr(0.0f));
    }

    template <typename HideLive, typename HideCached>
    static void updateCacheValidity(GrenadePredictionState& state, grenade_prediction_vars::LastTrajectoryVisibilityMode mode, float duration, bool hasCurtime, float curtime,
        bool projectilePresent, HideLive&& hideLive, HideCached&& hideCached) noexcept
    {
        if (state.cacheVisibility(mode, duration, hasCurtime, curtime, projectilePresent) != LastGrenadeCacheVisibility::Invalidate)
            return;
        state.invalidateCommittedTrajectory();
        if (state.rollbackDetected) {
            state.rollbackDetected = false;
            hideLive();
        }
        hideCached();
    }

    template <typename Draw, typename Hide>
    static void renderLastCommittedTrajectory(GrenadePredictionState& state, grenade_prediction_vars::LastTrajectoryVisibilityMode mode, float duration, bool hasCurtime,
        float curtime, bool projectilePresent, Draw&& draw, Hide&& hide) noexcept
    {
        if (state.cacheVisibility(mode, duration, hasCurtime, curtime, projectilePresent) == LastGrenadeCacheVisibility::Show)
            draw();
        else
            hide();
    }

    static void resetPresentationState(GrenadePredictionState& state) noexcept
    {
        state.livePresentationState = {};
        state.lastCachePresentationState = {};
    }

    static bool observeHeldThrow(GrenadeThrowObservation& observation, const void* weapon, bool pinPulled) noexcept
    {
        return observation.observePinState(weapon, pinPulled);
    }

    static void captureThrowStrength(GrenadeThrowObservation& observation, bool pinPulled, Optional<float> throwTime, auto&& readThrowStrength) noexcept
    {
        if (observation.isStrengthLocked() || (!pinPulled && !throwTime.greaterThan(0.0f).valueOr(false)))
            return;
        const auto throwStrength = readThrowStrength();
        if (throwStrength.hasValue())
            observation.retainThrowStrength(throwStrength.value());
    }

    static bool observeHeldThrow(GrenadeThrowObservation& observation, const void* weapon, bool pinPulled, auto&& readThrowStrength) noexcept
    {
        const bool releaseEdge = observeHeldThrow(observation, weapon, pinPulled);
        captureThrowStrength(observation, pinPulled, {}, readThrowStrength);
        return releaseEdge;
    }

    static bool completeHeldThrow(GrenadePredictionState& state, const void* weapon, bool hasCurtime, float curtime) noexcept
    {
        if (!state.throwObservation.consumeActualExecution(hasCurtime, curtime))
            return false;

        const bool stagedTrajectoryReady = state.throwObservation.canCommitActualExecution()
            && state.stageOwnedTempTrajectory(weapon, state.throwObservation.pendingSequence());
        state.finalizeStagedTrajectory(stagedTrajectoryReady, hasCurtime, curtime);
        state.invalidateTempTrajectory();
        return true;
    }

    static bool completeLegacyHeldThrow(GrenadePredictionState& state, const void* weapon, bool releaseEdge, bool hasCurtime, float curtime) noexcept
    {
        if (!state.throwObservation.consumeLegacyRelease(releaseEdge))
            return false;

        const bool stagedTrajectoryReady = state.throwObservation.canCommitActualExecution()
            && state.stageOwnedTempTrajectory(weapon, state.throwObservation.pendingSequence());
        state.finalizeStagedTrajectory(stagedTrajectoryReady, hasCurtime, curtime);
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
        if (!projectile.hasValue() || !state.liveGrenadeAuthority.observeForSimulation(projectile.value()) || !simulate(projectile.value()))
            return false;

        state.commitLiveGrenadeTrajectory();
        state.finalizeStagedTrajectory(true, currentTime.hasValue(), currentTime.valueOr(0.0f));
        state.liveGrenadeAuthority.accept(projectile.value(), currentTime);
        state.invalidateTempTrajectory();
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
