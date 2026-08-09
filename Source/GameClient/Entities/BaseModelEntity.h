#pragma once

#include <type_traits>

#include <CS2/Classes/Entities/C_BaseModelEntity.h>
#include <GameClient/Entities/BaseEntity.h>
#include <GameClient/Entities/CollisionProperty.h>
#include <MemoryPatterns/PatternTypes/BaseModelEntityPatternTypes.h>

#include "GlowProperty.h"

template <typename HookContext>
class BaseModelEntity {
public:
    using RawType = cs2::C_BaseModelEntity;

    BaseModelEntity(HookContext& hookContext, cs2::C_BaseModelEntity* baseModelEntity) noexcept
        : hookContext{hookContext}
        , baseModelEntity{baseModelEntity}
    {
    }

    [[nodiscard]] decltype(auto) baseEntity() const noexcept
    {
        return hookContext.template make<BaseEntity>(baseModelEntity);
    }

    template <template <typename...> typename T>
    [[nodiscard]] decltype(auto) as() const noexcept
    {
        return hookContext.template make<T<HookContext>>(static_cast<typename T<HookContext>::RawType*>(baseModelEntity));
    }

    [[nodiscard]] decltype(auto) glowProperty() const noexcept
    {
        return hookContext.template make<GlowProperty>(hookContext.patternSearchResults().template get<OffsetToGlowProperty>().of(baseModelEntity).get());
    }

    [[nodiscard]] decltype(auto) collisionProperty() const noexcept
    {
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<OffsetToCollisionProperty>())
            return hookContext.template make<CollisionProperty>(hookContext.patternSearchResults().template get<OffsetToCollisionProperty>().of(baseModelEntity).get());
        else
            return hookContext.template make<CollisionProperty>(nullptr);
    }

private:
    HookContext& hookContext;
    cs2::C_BaseModelEntity* baseModelEntity;
};
