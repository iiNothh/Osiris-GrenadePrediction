#pragma once

#include <CS2/Classes/CCollisionProperty.h>
#include <MemoryPatterns/PatternTypes/CollisionPropertyPatternTypes.h>
#include <Utils/Optional.h>

template <typename HookContext>
class CollisionProperty {
public:
    CollisionProperty(HookContext& hookContext, cs2::CCollisionProperty* collisionProperty) noexcept
        : hookContext{hookContext}
        , collisionProperty{collisionProperty}
    {
    }

    [[nodiscard]] Optional<cs2::Vector> mins() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToCollisionMins>().of(collisionProperty).toOptional();
    }

    [[nodiscard]] Optional<cs2::Vector> maxs() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToCollisionMaxs>().of(collisionProperty).toOptional();
    }

private:
    HookContext& hookContext;
    cs2::CCollisionProperty* collisionProperty;
};
