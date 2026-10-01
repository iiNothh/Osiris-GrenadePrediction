#pragma once

#include <GameClient/GrenadePrediction/GrenadeLaunchState.h>
#include <Utils/Optional.h>

enum class GrenadeLaunchRoute {
    Unavailable,
    Manual,
    RetainedGathered,
    KnownUnpinnedFullStrength
};

[[nodiscard]] constexpr GrenadeLaunchRoute selectGrenadeLaunchRoute(bool hasRetainedThrowStrength, bool hasPinBaseline, bool pinPulled) noexcept
{
    if (!hasPinBaseline)
        return GrenadeLaunchRoute::Unavailable;
    if (hasRetainedThrowStrength)
        return GrenadeLaunchRoute::RetainedGathered;
    if (!pinPulled)
        return GrenadeLaunchRoute::KnownUnpinnedFullStrength;
    return GrenadeLaunchRoute::Manual;
}

template <typename RetainedGatheredProvider, typename KnownUnpinnedFullStrengthProvider, typename ManualProvider>
[[nodiscard]] Optional<GrenadeLaunchState> prepareGrenadeLaunch(bool finalized, GrenadeLaunchRoute route,
    RetainedGatheredProvider&& retainedGatheredProvider,
    KnownUnpinnedFullStrengthProvider&& knownUnpinnedFullStrengthProvider,
    ManualProvider&& manualProvider) noexcept
{
    if (finalized)
        return {};
    switch (route) {
    case GrenadeLaunchRoute::RetainedGathered: {
        auto retainedGatheredState = retainedGatheredProvider();
        if (retainedGatheredState.hasValue())
            return retainedGatheredState;
        return manualProvider();
    }
    case GrenadeLaunchRoute::KnownUnpinnedFullStrength:
        return knownUnpinnedFullStrengthProvider();
    case GrenadeLaunchRoute::Manual:
        return manualProvider();
    case GrenadeLaunchRoute::Unavailable:
    default:
        return {};
    }
}
