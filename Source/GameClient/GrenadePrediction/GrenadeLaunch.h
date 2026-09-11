#pragma once

#include <bit>
#include <cstdint>
#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Classes/Entities/WeaponEntities.h>
#include <CS2/Classes/EntitySystem/CEntityIdentity.h>
#include <CS2/Constants/EntityHandle.h>
#include <MemoryPatterns/PatternTypes/ClientPatternTypes.h>
#include <MemoryPatterns/PatternTypes/EntityPatternTypes.h>
#include <Utils/Optional.h>

#include "GrenadeLaunchState.h"

template <typename HookContext>
class GrenadeLaunch {
public:
    explicit GrenadeLaunch(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    [[nodiscard]] Optional<GrenadeLaunchState> get(cs2::C_BaseCSGrenade* grenade, cs2::C_CSPlayerPawn* pawn) const noexcept
    {
        if (grenade == nullptr || pawn == nullptr || pawn->identity == nullptr)
            return {};
        const auto& results = hookContext.patternSearchResults();
        const auto buildLaunch = results.template get<BuildGrenadeLaunchFunction>();
        const auto owner = results.template get<OffsetToOwnerEntity>().of(static_cast<cs2::C_BaseEntity*>(grenade)).toOptional();
        const auto pawnHandle = pawn->identity->handle;
        if (buildLaunch == nullptr || !owner.hasValue() || owner.value() == cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}
            || pawnHandle == cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX} || owner.value() != pawnHandle)
            return {};
        const auto nonFinite = std::bit_cast<float>(std::uint32_t{0x7FC00000u});
        cs2::Vector origin{nonFinite, nonFinite, nonFinite};
        cs2::Vector velocity{nonFinite, nonFinite, nonFinite};
        static_cast<void>(buildLaunch(grenade, pawn, &origin, &velocity, false));
        if (origin.isFinite() && velocity.isFinite())
            return GrenadeLaunchState{origin, velocity};
        return {};
    }

private:
    HookContext& hookContext;
};
