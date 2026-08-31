#pragma once

#include <GameClient/GrenadePrediction/GrenadeLaunch.h>
#include <Utils/Optional.h>

enum class GrenadeLaunchPreparationStatus { Unavailable, Ready };

struct GrenadeLaunchPreparation {
    GrenadeLaunchPreparationStatus status;
    Optional<GrenadeLaunchState> state;

};

template <typename NativeProvider, typename ManualProvider>
[[nodiscard]] GrenadeLaunchPreparation prepareGrenadeLaunch(bool finalized, bool nativeLaunchEligible, NativeProvider&& nativeProvider, ManualProvider&& manualProvider) noexcept
{
    if (finalized)
        return {GrenadeLaunchPreparationStatus::Unavailable, {}};
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
