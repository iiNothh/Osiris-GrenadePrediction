#pragma once

#include <Features/Visuals/GrenadePrediction/GrenadePredictionConfigVariables.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Utils/Math.h>

class CachedGrenadeTrajectoryPresentation {
public:
    enum class Decision { Hide, Show, Invalidate };

    [[nodiscard]] static Decision decide(const GrenadePredictionState& state, grenade_prediction_vars::LastTrajectoryVisibilityMode mode, float duration,
        bool hasCurtime, float curtime, bool projectilePresent) noexcept
    {
        hasCurtime = hasCurtime && Math::isFinite(curtime);
        mode = grenade_prediction_vars::normalizeLastTrajectoryVisibilityMode(static_cast<std::uint8_t>(mode));
        if (mode == grenade_prediction_vars::LastTrajectoryVisibilityMode::Off)
            return Decision::Invalidate;
        if (mode == grenade_prediction_vars::LastTrajectoryVisibilityMode::Always)
            return state.lastCommittedTrajectory.valid && state.lastCommittedTrajectory.pointsCount ? Decision::Show : Decision::Hide;
        if (mode == grenade_prediction_vars::LastTrajectoryVisibilityMode::Explode)
            return state.lastCommittedTrajectory.valid && state.lastCommittedTrajectory.pointsCount && projectilePresent ? Decision::Show : Decision::Invalidate;
        duration = grenade_prediction_vars::normalizeCacheDuration(duration);
        if (!(duration > 0.0f))
            return Decision::Invalidate;
        if (!state.lastCommittedTrajectory.valid || !state.lastCommittedTrajectory.pointsCount || !hasCurtime || !state.hasCommitCurtime)
            return Decision::Hide;
        if (state.frameCommitMarker == state.frame)
            return Decision::Show;
        return curtime - state.lastCommitCurtime <= duration ? Decision::Show : Decision::Invalidate;
    }

    template <typename Draw, typename HideLive, typename HideCached>
    static void apply(GrenadePredictionState& state, Decision decision, Draw&& draw, HideLive&& hideLive, HideCached&& hideCached) noexcept
    {
        if (decision == Decision::Show) {
            draw();
            return;
        }

        if (decision == Decision::Invalidate && state.frameCommitMarker != state.frame) {
            state.invalidateCommittedTrajectory();
        }
        if (state.rollbackDetected) {
            state.rollbackDetected = false;
            hideLive();
        }
        hideCached();
    }
};
