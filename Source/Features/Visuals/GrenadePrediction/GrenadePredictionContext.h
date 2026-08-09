#pragma once

template <typename HookContext>
class GrenadePredictionContext {
public:
    explicit GrenadePredictionContext(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    [[nodiscard]] auto& state() const noexcept
    {
        return hookContext.featuresStates().visualFeaturesStates.grenadePredictionState;
    }

    [[nodiscard]] auto& config() const noexcept
    {
        return hookContext.config();
    }

private:
    HookContext& hookContext;
};
