#pragma once

#include <CS2/Classes/Entities/GrenadeProjectiles.h>
#include <MemoryPatterns/PatternTypes/DecoyProjectilePatternTypes.h>
#include <Utils/Optional.h>

template <typename HookContext>
class DecoyProjectile {
public:
    using RawType = cs2::C_DecoyProjectile;

    DecoyProjectile(HookContext& hookContext, cs2::C_DecoyProjectile* decoyProjectile) noexcept
        : hookContext{hookContext}
        , decoyProjectile{decoyProjectile}
    {
    }

    [[nodiscard]] Optional<std::int32_t> decoyShotTick() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToDecoyShotTick>().of(decoyProjectile).toOptional();
    }

private:
    HookContext& hookContext;
    cs2::C_DecoyProjectile* decoyProjectile;
};
