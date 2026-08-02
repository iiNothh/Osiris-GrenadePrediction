#pragma once

#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Held/HeldGrenadePrediction.h>

template <typename HookContext>
class GrenadePrediction {
public:
    explicit GrenadePrediction(HookContext& hookContext) noexcept
        : heldPrediction{hookContext, hookContext.featuresStates().visualFeaturesStates.grenadePredictionState.held}
    {
    }

    void run() noexcept
    {
        heldPrediction.run();
    }

    void clearPrediction() noexcept
    {
        heldPrediction.clear();
    }

private:
    grenade_prediction::HeldGrenadePrediction<HookContext> heldPrediction;
};
