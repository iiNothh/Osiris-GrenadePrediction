#pragma once

#include <type_traits>

#include <CS2/Classes/Entities/C_BaseCSGrenadeProjectile.h>
#include <CS2/Constants/EntityHandle.h>
#include <MemoryPatterns/PatternTypes/GrenadeProjectilePatternTypes.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

template <typename HookContext>
class GrenadeProjectile {
public:
    using RawType = cs2::C_BaseCSGrenadeProjectile;

    GrenadeProjectile(HookContext& hookContext, cs2::C_BaseCSGrenadeProjectile* grenadeProjectile) noexcept
        : hookContext{hookContext}
        , grenadeProjectile{grenadeProjectile}
    {
    }

    [[nodiscard]] Optional<cs2::Vector> initialPosition() const noexcept
    {
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<OffsetToGrenadeInitialPosition>()) {
            return finite(hookContext.patternSearchResults().template get<OffsetToGrenadeInitialPosition>().of(grenadeProjectile).toOptional());
        } else return {};
    }

    [[nodiscard]] Optional<cs2::Vector> initialVelocity() const noexcept
    {
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<OffsetToGrenadeInitialVelocity>()) {
            return finite(hookContext.patternSearchResults().template get<OffsetToGrenadeInitialVelocity>().of(grenadeProjectile).toOptional());
        } else return {};
    }

    [[nodiscard]] Optional<cs2::CEntityHandle> thrower() const noexcept
    {
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<OffsetToGrenadeThrower>()) {
            const auto thrower = hookContext.patternSearchResults().template get<OffsetToGrenadeThrower>()
                .of(static_cast<cs2::C_BaseGrenade*>(grenadeProjectile)).toOptional();
            if (thrower.hasValue() && thrower.value().value != cs2::INVALID_EHANDLE_INDEX)
                return thrower;
        }
        return {};
    }

    [[nodiscard]] Optional<std::int32_t> explodeEffectTickBegin() const noexcept
    {
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<OffsetToExplodeEffectTickBegin>())
            return hookContext.patternSearchResults().template get<OffsetToExplodeEffectTickBegin>().of(grenadeProjectile).toOptional();
        else return {};
    }

private:
    [[nodiscard]] static Optional<cs2::Vector> finite(Optional<cs2::Vector> value) noexcept
    {
        if (!value.hasValue() || !Math::isFinite(value.value().x) || !Math::isFinite(value.value().y) || !Math::isFinite(value.value().z))
            return {};
        return value;
    }

    HookContext& hookContext;
    cs2::C_BaseCSGrenadeProjectile* grenadeProjectile;
};
