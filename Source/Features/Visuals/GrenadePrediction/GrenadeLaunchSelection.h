#pragma once

#include <GameClient/GrenadePrediction/GrenadeLaunch.h>
#include <Utils/Optional.h>

enum class GrenadeLaunchPreparationStatus { Event, Unscheduled, Finalized, Unavailable, Ready };

struct GrenadeLaunchPreparation {
    GrenadeLaunchPreparationStatus status;
    Optional<GrenadeLaunchState> state;

};

template <typename NativeProvider, typename ManualProvider>
[[nodiscard]] GrenadeLaunchPreparation prepareGrenadeLaunch(bool event, bool shouldUpdate, bool finalized, bool nativeLaunchEligible, NativeProvider&& nativeProvider, ManualProvider&& manualProvider) noexcept
{
    if (event)
        return {GrenadeLaunchPreparationStatus::Event, {}};
    if (!shouldUpdate)
        return {GrenadeLaunchPreparationStatus::Unscheduled, {}};
    if (finalized)
        return {GrenadeLaunchPreparationStatus::Finalized, {}};
    if (nativeLaunchEligible) {
        auto nativeState = nativeProvider();
        if (nativeState.hasValue())
            return {GrenadeLaunchPreparationStatus::Ready, nativeState};
    }
    auto manualState = manualProvider();
    return manualState.hasValue()
        ? GrenadeLaunchPreparation{GrenadeLaunchPreparationStatus::Ready, manualState}
        : GrenadeLaunchPreparation{GrenadeLaunchPreparationStatus::Unavailable, {}};
}
