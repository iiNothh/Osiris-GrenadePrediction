#pragma once

#include <GameClient/GrenadePrediction/GrenadeLaunchState.h>
#include <Utils/Optional.h>

template <typename NativeProvider, typename ManualProvider>
[[nodiscard]] Optional<GrenadeLaunchState> prepareGrenadeLaunch(bool finalized, bool nativeLaunchEligible, NativeProvider&& nativeProvider, ManualProvider&& manualProvider) noexcept
{
    if (finalized)
        return {};
    if (nativeLaunchEligible) {
        auto nativeState = nativeProvider();
        if (nativeState.hasValue())
            return nativeState;
    }
    return manualProvider();
}
