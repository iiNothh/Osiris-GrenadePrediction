#pragma once

#include <CS2/Classes/Entities/C_Inferno.h>
#include <MemoryPatterns/PatternTypes/InfernoPatternTypes.h>

template <typename HookContext>
class Inferno {
public:
    using RawType = cs2::C_Inferno;

    Inferno(HookContext& hookContext, cs2::C_Inferno* inferno) noexcept
        : hookContext{hookContext}
        , inferno{inferno}
    {
    }

    [[nodiscard]] auto firePositions() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToFirePositions>().arrayOf(inferno);
    }

    [[nodiscard]] auto fireIsBurning() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToFireIsBurning>().arrayOf(inferno);
    }

    [[nodiscard]] auto fireCount() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToFireCount>().of(inferno).toOptional();
    }

    [[nodiscard]] auto fireLifetime() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToFireLifetime>().of(inferno).toOptional();
    }

private:
    HookContext& hookContext;
    cs2::C_Inferno* inferno;
};
