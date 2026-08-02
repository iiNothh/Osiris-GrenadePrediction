#pragma once

#include <bit>
#include <cstdint>

#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Classes/Entities/C_CSWeaponBase.h>
#include <CS2/Constants/EntityHandle.h>
#include <GameClient/GrenadePrediction/GrenadeLaunchState.h>
#include <MemoryPatterns/PatternTypes/EntityPatternTypes.h>
#include <Platform/Macros/IsPlatform.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

#if IS_WIN64()
#include <MemoryPatterns/PatternTypes/ClientPatternTypes.h>
#endif

template <typename HookContext>
class GrenadeLaunch {
public:
    explicit GrenadeLaunch(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    [[nodiscard]] Optional<grenade_prediction::GrenadeLaunchState> get(cs2::C_CSWeaponBase* weapon, cs2::C_CSPlayerPawn* playerPawn) const noexcept
    {
#if IS_WIN64()
        if (weapon == nullptr || playerPawn == nullptr || playerPawn->identity == nullptr)
            return {};
        if (playerPawn->identity->handle == cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX})
            return {};

        const auto& results = hookContext.patternSearchResults();
        const auto buildGrenadeLaunch = results.template get<BuildGrenadeLaunchFunction>();
        const auto offsetToOwnerEntity = results.template get<OffsetToOwnerEntity>();
        if (buildGrenadeLaunch == nullptr || !offsetToOwnerEntity)
            return {};

        const auto ownerHandle = offsetToOwnerEntity.of(weapon).toOptional();
        if (!ownerHandle.hasValue() || ownerHandle.value() == cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX} || ownerHandle.value() != playerPawn->identity->handle)
            return {};

        const auto nonFinite = std::bit_cast<float>(std::uint32_t{0x7F800000u});
        grenade_prediction::GrenadeLaunchState launchState{{nonFinite, nonFinite, nonFinite}, {nonFinite, nonFinite, nonFinite}};
        buildGrenadeLaunch(weapon, playerPawn, &launchState.origin, &launchState.velocity, false);
        if (!Math::isFinite(launchState.origin.x) || !Math::isFinite(launchState.origin.y) || !Math::isFinite(launchState.origin.z)
            || !Math::isFinite(launchState.velocity.x) || !Math::isFinite(launchState.velocity.y) || !Math::isFinite(launchState.velocity.z))
            return {};

        return launchState;
#else
        (void)weapon;
        (void)playerPawn;
        return {};
#endif
    }

private:
    HookContext& hookContext;
};
