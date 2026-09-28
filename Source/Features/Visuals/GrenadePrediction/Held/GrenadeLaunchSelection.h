#pragma once

#include <GameClient/GrenadePrediction/GrenadeLaunchState.h>
#include <Utils/Optional.h>

enum class GrenadeLaunchRoute {
    Unavailable,
    Manual,
    Native,
    NativeUnpinned
};

[[nodiscard]] constexpr GrenadeLaunchRoute selectGrenadeLaunchRoute(bool hasRetainedThrowStrength, bool hasPinBaseline, bool pinPulled) noexcept
{
    if (!hasPinBaseline)
        return GrenadeLaunchRoute::Unavailable;
    if (hasRetainedThrowStrength)
        return GrenadeLaunchRoute::Native;
    if (!pinPulled)
        return GrenadeLaunchRoute::NativeUnpinned;
    return GrenadeLaunchRoute::Manual;
}

template <typename NativeProvider, typename NativeUnpinnedProvider, typename ManualProvider>
[[nodiscard]] Optional<GrenadeLaunchState> prepareGrenadeLaunch(bool finalized, GrenadeLaunchRoute route, NativeProvider&& nativeProvider,
    NativeUnpinnedProvider&& nativeUnpinnedProvider, ManualProvider&& manualProvider) noexcept
{
    if (finalized)
        return {};
    if (route == GrenadeLaunchRoute::Native) {
        auto nativeState = nativeProvider();
        if (nativeState.hasValue())
            return nativeState;
        return manualProvider();
    }
    if (route == GrenadeLaunchRoute::NativeUnpinned)
        return nativeUnpinnedProvider();
    if (route == GrenadeLaunchRoute::Manual)
        return manualProvider();
    return {};
}
