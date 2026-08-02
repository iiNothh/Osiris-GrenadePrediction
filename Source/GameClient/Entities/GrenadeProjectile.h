#pragma once

#include <CS2/Classes/Entities/C_BaseCSGrenadeProjectile.h>
#include <MemoryPatterns/PatternTypes/GrenadeProjectilePatternTypes.h>
#include <Utils/Math.h>

template <typename HookContext>
class GrenadeProjectile {
public:
    using RawType = cs2::C_BaseCSGrenadeProjectile;

    GrenadeProjectile(HookContext& hookContext, RawType* grenadeProjectile) noexcept
        : hookContext{hookContext}
        , grenadeProjectile{grenadeProjectile}
    {
    }

    [[nodiscard]] Optional<cs2::Vector> initialPosition() const noexcept { return vector<OffsetToInitialPosition>(); }
    [[nodiscard]] Optional<cs2::Vector> initialVelocity() const noexcept { return vector<OffsetToInitialVelocity>(); }
    [[nodiscard]] Optional<cs2::CHandle<cs2::C_CSPlayerPawn>> thrower() const noexcept
    {
        const auto handle = hookContext.patternSearchResults().template get<OffsetToThrower>().of(grenadeProjectile).toOptional();
        if (handle.hasValue() && handle.value().value != cs2::INVALID_EHANDLE_INDEX)
            return handle;
        return {};
    }

private:
    template <typename Offset>
    [[nodiscard]] Optional<cs2::Vector> vector() const noexcept
    {
        const auto value = hookContext.patternSearchResults().template get<Offset>().of(grenadeProjectile).toOptional();
        if (value.hasValue() && Math::isFinite(value.value().x) && Math::isFinite(value.value().y) && Math::isFinite(value.value().z))
            return value;
        return {};
    }

    HookContext& hookContext;
    RawType* grenadeProjectile;
};
