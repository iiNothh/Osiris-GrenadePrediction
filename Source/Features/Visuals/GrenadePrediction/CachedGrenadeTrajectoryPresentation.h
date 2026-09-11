#pragma once

#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>

class CachedGrenadeTrajectoryPresentation {
public:
    template <typename Draw, typename HideLive, typename HideCached>
    static void apply(GrenadePredictionState& state, LastGrenadeCacheVisibility decision, Draw&& draw, HideLive&& hideLive, HideCached&& hideCached) noexcept
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
};
