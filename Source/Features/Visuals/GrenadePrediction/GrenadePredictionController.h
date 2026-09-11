#pragma once

#include <GameClient/Entities/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheUpdater.h>
#include <Utils/Math.h>

class GrenadePredictionController {
public:
    static void beginFrame(GrenadePredictionState& state) noexcept
    {
        state.beginFrame();
    }

    static void clearPredictionState(GrenadePredictionState& state) noexcept
    {
        state.throwObservation.reset();
        state.updateScheduler.reset();
        state.liveGrenadeAuthority.reset();
        state.lastCommitCurtime = 0.0f;
        state.lastValidCurtime = 0.0f;
        state.hasCommitCurtime = false;
        state.hasLastValidCurtime = false;
        state.rollbackDetected = false;
        state.invalidateTempTrajectory();
        state.invalidateCommittedTrajectory();
    }

    template <typename HideLive, typename HideCached>
    static void clearPredictionAndHidePanels(GrenadePredictionState& state, HideLive&& hideLive, HideCached&& hideCached) noexcept
    {
        clearPredictionState(state);
        hideLive();
        hideCached();
    }

    [[nodiscard]] static bool observeCurrentTime(GrenadePredictionState& state, Optional<float> currentTime) noexcept
    {
        const bool hasCurrentTime = currentTime.hasValue() && Math::isFinite(currentTime.value());
        return state.observeTime(hasCurrentTime, hasCurrentTime ? currentTime.value() : 0.0f);
    }

    [[nodiscard]] static bool advanceScheduler(GrenadePredictionUpdateScheduler& scheduler, bool force, Optional<float> frametime) noexcept
    {
        const bool hasFrametime = frametime.hasValue() && Math::isFinite(frametime.value());
        return scheduler.shouldUpdate(force, hasFrametime, hasFrametime ? frametime.value() : 0.0f);
    }

    [[nodiscard]] static bool isHeldSimulationNecessary(const GrenadePredictionState& state, const HeldGrenadeSimulationInput& input, bool scheduled) noexcept
    {
        return scheduled && !state.liveGrenadeAuthority.blocksHeldPrediction() && state.shouldSimulateHeld(input);
    }

    static void recordHeldSimulationResult(GrenadePredictionState& state, const HeldGrenadeSimulationInput& input, bool succeeded) noexcept
    {
        if (succeeded && state.tempTrajectory.valid && state.tempTrajectory.pointsCount)
            state.cacheHeldSimulationInput(input);
        else
            state.invalidateTempTrajectory();
    }

    [[nodiscard]] static LastGrenadeCacheVisibility makeCachedTrajectoryPresentationDecision(const GrenadePredictionState& state,
        grenade_prediction_vars::LastTrajectoryVisibilityMode mode, float duration, bool hasCurtime, float curtime, bool projectilePresent) noexcept
    {
        return state.cacheVisibility(mode, duration, hasCurtime, curtime, projectilePresent);
    }

    template <typename Draw, typename HideLive, typename HideCached>
    static void applyCachedTrajectoryPresentationDecision(GrenadePredictionState& state, LastGrenadeCacheVisibility decision, Draw&& draw, HideLive&& hideLive,
        HideCached&& hideCached) noexcept
    {
        if (decision == LastGrenadeCacheVisibility::Show) {
            draw();
            return;
        }

        if (decision == LastGrenadeCacheVisibility::Invalidate && state.frameCommitMarker != state.frame) {
            state.invalidateCommittedTrajectory();
        }
        if (state.rollbackDetected) {
            state.rollbackDetected = false;
            hideLive();
        }
        hideCached();
    }

    static void resetPresentationState(GrenadePredictionState& state) noexcept
    {
        state.livePresentationState = {};
        state.lastCachePresentationState = {};
    }

    [[nodiscard]] static bool observeHeldThrow(GrenadeThrowObservation& observation, cs2::CEntityHandle weapon, bool pinPulled) noexcept
    {
        return observation.observePinState(weapon, pinPulled);
    }

    static void captureThrowStrength(GrenadeThrowObservation& observation, bool pinPulled, Optional<float> throwTime, auto&& readThrowStrength) noexcept
    {
        const bool hasPositiveThrowTime = throwTime.hasValue() && Math::isFinite(throwTime.value()) && throwTime.value() > 0.0f;
        if (observation.isStrengthLocked() || (!pinPulled && !hasPositiveThrowTime))
            return;
        const auto throwStrength = readThrowStrength();
        if (throwStrength.hasValue())
            observation.retainThrowStrength(throwStrength.value());
    }

    [[nodiscard]] static bool completeHeldThrow(GrenadePredictionState& state, cs2::CEntityHandle weapon, bool hasCurtime, float curtime) noexcept
    {
        if (!state.throwObservation.consumeActualExecution(hasCurtime, curtime))
            return false;

        const bool stagedTrajectoryReady = state.throwObservation.canCommitActualExecution()
            && state.stageOwnedTempTrajectory(weapon, state.throwObservation.pendingSequence());
        state.finalizeStagedTrajectory(stagedTrajectoryReady, hasCurtime, curtime);
        state.invalidateTempTrajectory();
        return true;
    }

    [[nodiscard]] static bool completeLegacyHeldThrow(GrenadePredictionState& state, cs2::CEntityHandle weapon, bool releaseEdge, bool hasCurtime, float curtime) noexcept
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
    [[nodiscard]] static bool acceptNewestLiveGrenade(GrenadePredictionState& state, Trajectory& liveGrenadeTrajectoryScratch,
        cs2::CEntityHandle localPawnHandle, Optional<float> currentTime, Simulate&& simulate) noexcept
    {
        if (currentTime.hasValue() && !Math::isFinite(currentTime.value()))
            currentTime = {};
        state.liveGrenadeAuthority.observeLocalPawn(localPawnHandle);
        state.liveGrenadeAuthority.update(state.liveGrenadeCache);
        if (!state.liveGrenadeCache.hasAuthoritativeScan()) {
            return false;
        }

        const auto projectile = state.liveGrenadeAuthority.newestLocalProjectile(state.liveGrenadeCache);
        if (!projectile.hasValue() || !state.liveGrenadeAuthority.observeForSimulation(projectile.value()))
            return false;
        if (!state.liveGrenadeAuthority.isSimulationRetryDue(projectile.value(), state.frame)) {
            return false;
        }
        if (!simulate(projectile.value())) {
            state.liveGrenadeAuthority.recordSimulationFailure(projectile.value(), state.frame);
            return false;
        }

        state.commitLiveGrenadeTrajectory(liveGrenadeTrajectoryScratch);
        state.finalizeStagedTrajectory(true, currentTime.hasValue(), currentTime.valueOr(0.0f));
        state.liveGrenadeAuthority.accept(projectile.value(), currentTime);
        state.invalidateTempTrajectory();
        return true;
    }

    template <typename Projectile, typename Decoy>
    [[nodiscard]] static bool updateDecoyLiveGrenade(LiveGrenadeCache& cache, const Projectile& projectile, cs2::CEntityHandle projectileHandle, const Decoy& decoy) noexcept
    {
        return LiveGrenadeCacheUpdater{cache}.update(projectile, projectileHandle, GrenadeKind::Decoy, {.decoyShotTick = decoy.decoyShotTick()});
    }

    template <typename Projectile>
    [[nodiscard]] static bool updateHELiveGrenade(LiveGrenadeCache& cache, const Projectile& projectile, cs2::CEntityHandle projectileHandle) noexcept
    {
        return LiveGrenadeCacheUpdater{cache}.update(projectile, projectileHandle, GrenadeKind::HEGrenade,
            {.heExplodeEffectTickBegin = projectile.explodeEffectTickBegin()});
    }
};
