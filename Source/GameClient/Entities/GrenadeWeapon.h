#pragma once

#include <CS2/Classes/Entities/WeaponEntities.h>
#include <MemoryPatterns/PatternTypes/WeaponPatternTypes.h>

template <typename HookContext>
class GrenadeWeapon {
public:
    using RawType = cs2::C_BaseCSGrenade;

    GrenadeWeapon(HookContext& hookContext, cs2::C_BaseCSGrenade* grenadeWeapon) noexcept
        : hookContext{hookContext}
        , grenadeWeapon{grenadeWeapon}
    {
    }

    [[nodiscard]] auto throwTime() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToThrowTime>().of(grenadeWeapon).toOptional();
    }

    [[nodiscard]] auto pinPulled() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToPinPulled>().of(grenadeWeapon).toOptional();
    }

    [[nodiscard]] auto throwStrength() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToThrowStrength>().of(grenadeWeapon).toOptional();
    }

private:
    HookContext& hookContext;
    cs2::C_BaseCSGrenade* grenadeWeapon;
};
