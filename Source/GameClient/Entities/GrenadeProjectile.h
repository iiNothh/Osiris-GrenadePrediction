#pragma once

#include <type_traits>

#include <CS2/Classes/Entities/C_BaseCSGrenadeProjectile.h>
#include <CS2/Constants/EntityHandle.h>
#include <GameClient/Entities/BaseEntity.h>
#include <GameClient/Entities/GrenadeProjectileSample.h>
#include <GameClient/GlobalVars.h>
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

    [[nodiscard]] Optional<GrenadeProjectileSample> currentSample() const noexcept
    {
        const auto entity = BaseEntity<HookContext>{hookContext, static_cast<cs2::C_BaseEntity*>(grenadeProjectile)};
        const auto origin = entity.absOrigin();
        const auto velocity = entity.serverVelocity();
        const auto createTime = entity.createTime();
        const auto globalVars = hookContext.globalVars();
        const auto worldTime = globalVars.curtime();
        const auto worldTickInterval = globalVars.tickInterval();
        const auto worldTickCount = globalVars.tickCount();
        if (!origin.hasValue() || !velocity.hasValue() || !createTime.hasValue() || !worldTime.hasValue() || !worldTickInterval.hasValue() || !worldTickCount.hasValue()
            || !finite(origin.value()) || !Math::isFinite(worldTime.value()))
            return {};
        return GrenadeProjectileSample{origin.value(), velocity.value(), worldTime.value(), createTime.value(), worldTickInterval.value(), worldTickCount.value()};
    }

private:
    [[nodiscard]] static Optional<cs2::Vector> finite(Optional<cs2::Vector> value) noexcept
    {
        if (!value.hasValue() || !Math::isFinite(value.value().x) || !Math::isFinite(value.value().y) || !Math::isFinite(value.value().z))
            return {};
        return value;
    }

    [[nodiscard]] static bool finite(const cs2::Vector& value) noexcept
    {
        return Math::isFinite(value.x) && Math::isFinite(value.y) && Math::isFinite(value.z);
    }

    HookContext& hookContext;
    cs2::C_BaseCSGrenadeProjectile* grenadeProjectile;
};
