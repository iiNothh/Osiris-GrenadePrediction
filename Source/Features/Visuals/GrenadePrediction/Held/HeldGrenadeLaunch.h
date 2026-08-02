#pragma once

#include <GameClient/GrenadePrediction/GrenadeLaunchState.h>
#include <Features/Visuals/GrenadePrediction/Held/HeldGrenadeKind.h>
#include <Utils/Optional.h>

namespace grenade_prediction
{

struct HeldGrenadeLaunchData {
    GrenadeKind kind{GrenadeKind::None};
    GrenadeLaunchState launchState{};
};

class HeldGrenadeLaunch {
public:
    template <typename PlayerPawn, typename NativeLaunchProvider>
    [[nodiscard]] Optional<HeldGrenadeLaunchData> get(const PlayerPawn& playerPawn, NativeLaunchProvider& nativeLaunchProvider) const noexcept
    {
        const auto activeWeapon = playerPawn.getActiveWeapon();
        if (activeWeapon.raw() == nullptr)
            return {};
        const auto kind = heldGrenadeKind(activeWeapon.baseEntity().classify());
        if (kind == GrenadeKind::None)
            return {};
        const auto launchState = nativeLaunchProvider.get(activeWeapon.raw(), playerPawn.raw());
        if (!launchState.hasValue())
            return {};
        return HeldGrenadeLaunchData{kind, launchState.value()};
    }
};

}
