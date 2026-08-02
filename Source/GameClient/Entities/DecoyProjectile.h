#pragma once

#include <CS2/Classes/Entities/GrenadeProjectiles.h>
#include <MemoryPatterns/PatternTypes/DecoyProjectilePatternTypes.h>

template <typename HookContext>
class DecoyProjectile {
public:
    using RawType = cs2::C_DecoyProjectile;

    DecoyProjectile(HookContext& hookContext, RawType* decoyProjectile) noexcept
        : hookContext{hookContext}
        , decoyProjectile{decoyProjectile}
    {
    }

    [[nodiscard]] auto shotTick() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToDecoyShotTick>().of(decoyProjectile).toOptional();
    }

private:
    HookContext& hookContext;
    RawType* decoyProjectile;
};
