#pragma once

#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionState.h>
#include <Features/Visuals/GrenadePrediction/Trajectory.h>

struct GrenadePredictionPerHookState {
    Trajectory liveGrenadeTrajectoryScratch{};
    GrenadePlayerCollisionCollectionScratch playerCollisionCollectionScratch{};

    void clear() noexcept
    {
        liveGrenadeTrajectoryScratch.clear();
        playerCollisionCollectionScratch.reset();
    }
};
