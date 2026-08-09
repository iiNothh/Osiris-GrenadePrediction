#pragma once

#include <cstdint>

#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionMirror.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionConfigVariables.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadePredictionUpdateScheduler.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeThrowObservation.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeAuthority.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>
#include <Features/Visuals/GrenadePrediction/Rendering/GrenadeTrajectoryRenderer.h>
#include <Features/Visuals/GrenadePrediction/Trajectory.h>

enum class LastGrenadeCacheVisibility { Hide, Show, Invalidate };

struct GrenadePredictionState {
    Trajectory lastCommittedTrajectory{};
    Trajectory tempTrajectory{};
    Trajectory liveGrenadeTrajectoryScratch{};

    const void* tempTrajectoryWeapon{};
    std::uint32_t tempTrajectorySequence{};

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
    bool hasCommitCurtime{};

    void invalidateTempTrajectory() noexcept { tempTrajectory.clear(); tempTrajectoryWeapon = nullptr; tempTrajectorySequence = 0; }
    void invalidateCommittedTrajectory() noexcept { lastCommittedTrajectory.clear(); }
    void tagTempTrajectory(const void* weapon, std::uint32_t sequence) noexcept { tempTrajectoryWeapon = weapon; tempTrajectorySequence = sequence; }
    [[nodiscard]] bool ownsTempTrajectory(const void* weapon, std::uint32_t sequence) const noexcept
    {
        return tempTrajectory.valid && tempTrajectory.pointsCount && tempTrajectoryWeapon == weapon && tempTrajectorySequence == sequence;
    }
    void commitTempTrajectory(float curtime, bool hasCurtime) noexcept
    {
        copyTrajectory(lastCommittedTrajectory, tempTrajectory);
        if (hasCurtime) { lastCommitCurtime = curtime; hasCommitCurtime = true; }
    }
    void commitLiveTrajectory(float curtime, bool hasCurtime) noexcept
    {
        copyTrajectory(lastCommittedTrajectory, liveGrenadeTrajectoryScratch);
        if (hasCurtime) { lastCommitCurtime = curtime; hasCommitCurtime = true; }
    }
    [[nodiscard]] LastGrenadeCacheVisibility cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode mode, float duration, bool hasCurtime, float curtime, bool projectilePresent) noexcept
    {
        mode = grenade_prediction_vars::normalizeLastTrajectoryVisibilityMode(static_cast<std::uint8_t>(mode));
        if (mode == grenade_prediction_vars::LastTrajectoryVisibilityMode::Off)
            return LastGrenadeCacheVisibility::Invalidate;
        if (!lastCommittedTrajectory.valid || !lastCommittedTrajectory.pointsCount)
            return LastGrenadeCacheVisibility::Hide;
        if (mode == grenade_prediction_vars::LastTrajectoryVisibilityMode::Always)
            return LastGrenadeCacheVisibility::Show;
        if (mode == grenade_prediction_vars::LastTrajectoryVisibilityMode::Explode)
            return projectilePresent ? LastGrenadeCacheVisibility::Show : LastGrenadeCacheVisibility::Invalidate;
        duration = grenade_prediction_vars::normalizeCacheDuration(duration);
        return hasCurtime && hasCommitCurtime && duration > 0.0f && curtime - lastCommitCurtime <= duration ? LastGrenadeCacheVisibility::Show : LastGrenadeCacheVisibility::Invalidate;
    }

private:
    static void copyTrajectory(Trajectory& destination, const Trajectory& source) noexcept
    {
        destination.pointsCount = source.pointsCount;
        for (int i{}; i < source.pointsCount; ++i) destination.points[i] = source.points[i];
        destination.markersCount = source.markersCount;
        destination.worldContactMarkersCount = source.worldContactMarkersCount;
        for (int i{}; i < source.markersCount; ++i) destination.markers[i] = source.markers[i];
        destination.endPos = source.endPos;
        destination.valid = source.valid;
        destination.validLanding = source.validLanding;
    }
};
