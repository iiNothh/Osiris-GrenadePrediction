#pragma once

#include <GameClient/GrenadePrediction/GrenadeLaunch.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

enum class GrenadeLaunchPreparationStatus { Event, Unscheduled, Finalized, Unavailable, Ready };

struct GrenadeLaunchPreparation {
    GrenadeLaunchPreparationStatus status;
    Optional<GrenadeLaunchState> state;

    [[nodiscard]] bool shouldClearHeldOutput() const noexcept
    {
        return status == GrenadeLaunchPreparationStatus::Unavailable;
    }
};

[[nodiscard]] inline bool isValid(const GrenadeLaunchState& state) noexcept
{
    return Math::isFinite(state.origin.x) && Math::isFinite(state.origin.y) && Math::isFinite(state.origin.z)
        && Math::isFinite(state.velocity.x) && Math::isFinite(state.velocity.y) && Math::isFinite(state.velocity.z);
}

template <typename NativeProvider>
[[nodiscard]] GrenadeLaunchPreparation prepareGrenadeLaunch(bool event, bool shouldUpdate, bool finalized, bool nativeLaunchEligible, NativeProvider&& nativeProvider) noexcept
{
    if (event)
        return {GrenadeLaunchPreparationStatus::Event, {}};
    if (!shouldUpdate)
        return {GrenadeLaunchPreparationStatus::Unscheduled, {}};
    if (finalized)
        return {GrenadeLaunchPreparationStatus::Finalized, {}};
    if (!nativeLaunchEligible)
        return {GrenadeLaunchPreparationStatus::Unavailable, {}};

    const auto nativeState = nativeProvider();
    if (!nativeState.hasValue() || !isValid(nativeState.value()))
        return {GrenadeLaunchPreparationStatus::Unavailable, {}};
    return {GrenadeLaunchPreparationStatus::Ready, nativeState};
}
