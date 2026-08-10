#pragma once

#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheUpdater.h>
#include <Utils/Math.h>

class GrenadePredictionController {
public:
    static void beginFrame(GrenadePredictionState& state) noexcept
    {
        state.beginFrame();
    }

    [[nodiscard]] static bool observeCurrentTime(GrenadePredictionState& state, Optional<float> currentTime) noexcept
    {
        const bool hasCurrentTime = currentTime.hasValue() && Math::isFinite(currentTime.value());
        return state.observeTime(hasCurrentTime, hasCurrentTime ? currentTime.value() : 0.0f);
    }

    static void recordLiveCacheStatus(GrenadePredictionState& state) noexcept
    {
        if (state.liveGrenadeCache.hasOverflowed())
            state.diagnostics.record(GrenadePredictionDiagnosticKind::LiveCacheOverflow, state.frame);
    }

    static void recordCollisionSnapshotStatus(GrenadePredictionState& state) noexcept
    {
        const auto& scratch = state.playerCollisionCollectionScratch;
        if (scratch.overflowed)
            state.diagnostics.record(GrenadePredictionDiagnosticKind::CollisionOverflow, state.frame);
        if (scratch.playerDataInvalid || state.playerCollisionSnapshot.status == GrenadePlayerCollisionSnapshotStatus::Unavailable)
            state.diagnostics.record(GrenadePredictionDiagnosticKind::RelevantPlayerInvalid, state.frame);
        if (scratch.malformedUnrelatedIdentityCount || state.playerCollisionSnapshot.malformedUnrelatedIdentityCount)
            state.diagnostics.record(GrenadePredictionDiagnosticKind::MalformedUnrelatedIdentitySkipped, state.frame);
    }

    static bool advanceScheduler(GrenadePredictionUpdateScheduler& scheduler, bool force, Optional<float> frametime) noexcept
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

    // Compatibility wrappers for callers not yet migrated to one shared presentation decision.
    template <typename HideLive, typename HideCached>
    static void updateCacheValidity(GrenadePredictionState& state, grenade_prediction_vars::LastTrajectoryVisibilityMode mode, float duration, bool hasCurtime,
        float curtime, bool projectilePresent, HideLive&& hideLive, HideCached&& hideCached) noexcept
    {
        const auto decision = makeCachedTrajectoryPresentationDecision(state, mode, duration, hasCurtime, curtime, projectilePresent);
        applyCachedTrajectoryPresentationDecision(state, decision, []() noexcept {}, static_cast<HideLive&&>(hideLive), static_cast<HideCached&&>(hideCached));
    }

    template <typename Draw, typename Hide>
    static void renderLastCommittedTrajectory(GrenadePredictionState& state, grenade_prediction_vars::LastTrajectoryVisibilityMode mode, float duration, bool hasCurtime,
        float curtime, bool projectilePresent, Draw&& draw, Hide&& hide) noexcept
    {
        const auto decision = makeCachedTrajectoryPresentationDecision(state, mode, duration, hasCurtime, curtime, projectilePresent);
        applyCachedTrajectoryPresentationDecision(state, decision, static_cast<Draw&&>(draw), []() noexcept {}, static_cast<Hide&&>(hide));
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
        const bool hasPositiveThrowTime = throwTime.hasValue() && Math::isFinite(throwTime.value()) && throwTime.value() > 0.0f;
        if (observation.isStrengthLocked() || (!pinPulled && !hasPositiveThrowTime))
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
    static bool acceptNewestLiveGrenade(GrenadePredictionState& state, cs2::CEntityHandle localPawnHandle, Optional<float> currentTime, Simulate&& simulate) noexcept
    {
        if (currentTime.hasValue() && !Math::isFinite(currentTime.value()))
            currentTime = {};
        state.liveGrenadeAuthority.observeLocalPawn(localPawnHandle);
        state.liveGrenadeAuthority.update(state.liveGrenadeCache);
        if (!state.liveGrenadeCache.hasAuthoritativeScan()) {
            recordLiveCacheStatus(state);
            return false;
        }

        const auto projectile = state.liveGrenadeAuthority.newestLocalProjectile(state.liveGrenadeCache);
        if (!projectile.hasValue() || !state.liveGrenadeAuthority.observeForSimulation(projectile.value()))
            return false;
        if (!state.liveGrenadeAuthority.isSimulationRetryDue(projectile.value(), state.frame)) {
            state.diagnostics.record(GrenadePredictionDiagnosticKind::LiveSimulationBackoff, state.frame);
            return false;
        }
        if (!simulate(projectile.value())) {
            state.liveGrenadeAuthority.recordSimulationFailure(projectile.value(), state.frame);
            state.diagnostics.record(GrenadePredictionDiagnosticKind::LiveSimulationFailure, state.frame);
            return false;
        }

        state.commitLiveGrenadeTrajectory();
        state.finalizeStagedTrajectory(true, currentTime.hasValue(), currentTime.valueOr(0.0f));
        state.liveGrenadeAuthority.accept(projectile.value(), currentTime);
        state.invalidateTempTrajectory();
        return true;
    }

    template <typename Simulate>
    static bool completeLiveGrenadeScan(GrenadePredictionState& state, cs2::CEntityHandle localPawnHandle, Optional<float> currentTime, Simulate&& simulate) noexcept
    {
        LiveGrenadeCacheUpdater{state.liveGrenadeCache}.endScan();
        return acceptNewestLiveGrenade(state, localPawnHandle, currentTime, static_cast<Simulate&&>(simulate));
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
