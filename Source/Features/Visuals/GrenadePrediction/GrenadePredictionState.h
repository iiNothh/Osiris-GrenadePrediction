#pragma once

#include <cstdint>

#include <CS2/Classes/EntitySystem/CEntityHandle.h>
#include <CS2/Constants/EntityHandle.h>
#include <CS2/Panorama/PanelHandle.h>

#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionMirror.h>
#include <Features/Visuals/GrenadePrediction/GrenadeTrajectoryPresentationState.h>
#include <Features/Visuals/GrenadePrediction/Held/HeldGrenadeSimulationInput.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadePredictionUpdateScheduler.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeThrowObservation.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeAuthority.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>
#include <Features/Visuals/GrenadePrediction/Trajectory.h>
#include <Utils/Math.h>

struct GrenadePredictionState {
    Trajectory lastCommittedTrajectory{};
    Trajectory tempTrajectory{};

    cs2::CEntityHandle tempTrajectoryWeapon{cs2::INVALID_EHANDLE_INDEX};
    std::uint32_t tempTrajectorySequence{};
    HeldGrenadeSimulationInput heldSimulationInput{};
    bool hasHeldSimulationInput{};

    GrenadeThrowObservation throwObservation{};
    GrenadePredictionUpdateScheduler updateScheduler{};
    LiveGrenadeCache liveGrenadeCache{};
    LiveGrenadeAuthority liveGrenadeAuthority{};
    GrenadePlayerCollisionSnapshot playerCollisionSnapshot{};

    cs2::PanelHandle liveContainerPanelHandle{};
    cs2::PanelHandle lastCacheContainerPanelHandle{};

    GrenadeTrajectoryPresentationState livePresentationState{};
    GrenadeTrajectoryPresentationState lastCachePresentationState{};

    float lastCommitCurtime{};
    float lastValidCurtime{};
    bool hasCommitCurtime{};
    bool hasLastValidCurtime{};
    bool rollbackDetected{};
    std::uint32_t frame{};
    std::uint32_t timeObservationFrame{};
    std::uint32_t frameCommitMarker{};
    bool hasTimeObservationForFrame{};

    void beginFrame() noexcept
    {
        ++frame;
        hasTimeObservationForFrame = false;
    }

    [[nodiscard]] bool observeTime(bool hasCurtime, float curtime) noexcept
    {
        if (hasTimeObservationForFrame && timeObservationFrame == frame)
            return rollbackDetected;

        hasTimeObservationForFrame = true;
        timeObservationFrame = frame;
        rollbackDetected = false;
        hasCurtime = hasCurtime && Math::isFinite(curtime);
        if (!hasCurtime)
            return false;

        if ((hasLastValidCurtime && curtime < lastValidCurtime) || (hasCommitCurtime && curtime < lastCommitCurtime)) {
            resetForRollback();
            lastValidCurtime = curtime;
            hasLastValidCurtime = true;
            rollbackDetected = true;
            return true;
        }

        lastValidCurtime = curtime;
        hasLastValidCurtime = true;
        return false;
    }

    void invalidateHeldSimulationInput() noexcept
    {
        heldSimulationInput = {.weapon = cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}};
        hasHeldSimulationInput = false;
    }

    void invalidateTempTrajectory() noexcept
    {
        tempTrajectory.clear();
        tempTrajectoryWeapon = cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX};
        tempTrajectorySequence = 0;
        invalidateHeldSimulationInput();
    }

    void clearPrediction() noexcept
    {
        throwObservation.reset();
        updateScheduler.reset();
        liveGrenadeAuthority.reset();
        lastCommitCurtime = 0.0f;
        lastValidCurtime = 0.0f;
        hasCommitCurtime = false;
        hasLastValidCurtime = false;
        rollbackDetected = false;
        invalidateTempTrajectory();
        invalidateCommittedTrajectory();
    }
    void invalidateCommittedTrajectory() noexcept { lastCommittedTrajectory.clear(); }
    void tagTempTrajectory(cs2::CEntityHandle weapon, std::uint32_t sequence) noexcept
    {
        if (weapon == cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}) {
            invalidateTempTrajectory();
            return;
        }
        tempTrajectoryWeapon = weapon;
        tempTrajectorySequence = sequence;
    }
    void cacheHeldSimulationInput(const HeldGrenadeSimulationInput& input) noexcept
    {
        if (input.weapon == cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}) {
            invalidateTempTrajectory();
            return;
        }
        heldSimulationInput = input;
        hasHeldSimulationInput = true;
        tagTempTrajectory(input.weapon, input.throwSequence);
    }
    [[nodiscard]] bool shouldSimulateHeld(const HeldGrenadeSimulationInput& input) const noexcept
    {
        return input.weapon != cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}
            && (!hasHeldSimulationInput || !heldSimulationInput.exactlyEquals(input) || !ownsTempTrajectory(input.weapon, input.throwSequence));
    }
    [[nodiscard]] bool ownsTempTrajectory(cs2::CEntityHandle weapon, std::uint32_t sequence) const noexcept
    {
        return weapon != cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}
            && tempTrajectory.valid && tempTrajectory.pointsCount && tempTrajectoryWeapon == weapon && tempTrajectorySequence == sequence;
    }
    void recordHeldSimulationResult(const HeldGrenadeSimulationInput& input, bool succeeded) noexcept
    {
        if (succeeded && tempTrajectory.valid && tempTrajectory.pointsCount)
            cacheHeldSimulationInput(input);
        else
            invalidateTempTrajectory();
    }
    void commitLiveGrenadeTrajectory(const Trajectory& trajectory) noexcept
    {
        lastCommittedTrajectory.copyFrom(trajectory);
    }

    [[nodiscard]] bool stageOwnedTempTrajectory(cs2::CEntityHandle weapon, std::uint32_t sequence) noexcept
    {
        if (liveGrenadeAuthority.hasObservedLiveProjectile() || !ownsTempTrajectory(weapon, sequence))
            return false;
        lastCommittedTrajectory.copyFrom(tempTrajectory);
        return true;
    }
    void finalizeStagedTrajectory(bool hasCandidate, bool hasCurtime, float curtime) noexcept
    {
        if (!hasCandidate)
            return;
        if (hasCurtime && Math::isFinite(curtime)) { lastCommitCurtime = curtime; hasCommitCurtime = true; }
        else if (hasLastValidCurtime) { lastCommitCurtime = lastValidCurtime; hasCommitCurtime = true; }
        frameCommitMarker = frame;
    }
    [[nodiscard]] bool completeHeldThrow(cs2::CEntityHandle weapon, bool hasCurtime, float curtime) noexcept
    {
        if (!throwObservation.consumeActualExecution(hasCurtime, curtime))
            return false;

        const bool stagedTrajectoryReady = throwObservation.canCommitTrajectory()
            && stageOwnedTempTrajectory(weapon, throwObservation.pendingSequence());
        finalizeStagedTrajectory(stagedTrajectoryReady, hasCurtime, curtime);
        invalidateTempTrajectory();
        return true;
    }
    [[nodiscard]] bool completeLegacyHeldThrow(cs2::CEntityHandle weapon, bool releaseEdge, bool hasCurtime, float curtime) noexcept
    {
        if (!throwObservation.consumeLegacyRelease(releaseEdge))
            return false;

        const bool stagedTrajectoryReady = throwObservation.canCommitTrajectory()
            && stageOwnedTempTrajectory(weapon, throwObservation.pendingSequence());
        finalizeStagedTrajectory(stagedTrajectoryReady, hasCurtime, curtime);
        invalidateTempTrajectory();
        return true;
    }
    void resetPresentationState() noexcept
    {
        livePresentationState = {};
        lastCachePresentationState = {};
    }
    void resetForRollback() noexcept
    {
        throwObservation.reset();
        invalidateTempTrajectory();
        invalidateCommittedTrajectory();
        updateScheduler.reset();
        liveGrenadeAuthority.reset();
        lastCommitCurtime = 0.0f;
        lastValidCurtime = 0.0f;
        hasCommitCurtime = false;
        hasLastValidCurtime = false;
        frameCommitMarker = 0;
        rollbackDetected = true;
    }
};
