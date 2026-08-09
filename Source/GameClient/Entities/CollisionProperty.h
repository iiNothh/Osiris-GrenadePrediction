#pragma once

#include <type_traits>

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
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<OffsetToCollisionMins>())
            return hookContext.patternSearchResults().template get<OffsetToCollisionMins>().of(collisionProperty).toOptional();
        return {};
    }

    [[nodiscard]] Optional<cs2::Vector> maxs() const noexcept
    {
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<OffsetToCollisionMaxs>())
            return hookContext.patternSearchResults().template get<OffsetToCollisionMaxs>().of(collisionProperty).toOptional();
        return {};
    }

private:
    HookContext& hookContext;
    cs2::CCollisionProperty* collisionProperty;
};
