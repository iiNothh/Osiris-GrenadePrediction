#pragma once

#include <cstdint>
#include <type_traits>

#include <CS2/Classes/GlobalVars.h>
#include <MemoryPatterns/PatternTypes/GlobalVarsPatternTypes.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

template <typename HookContext>
struct GlobalVars {
    [[nodiscard]] Optional<float> curtime() const noexcept
    {
        if (globalVars)
            return globalVars->curtime;
        return {};
    }

    [[nodiscard]] Optional<float> frametime() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToFrametime>().of(globalVars).toOptional();
    }

    [[nodiscard]] Optional<std::int32_t> tickCount() const noexcept
    {
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<GlobalVarsTickCountOffset>())
            return hookContext.patternSearchResults().template get<GlobalVarsTickCountOffset>().of(globalVars).toOptional();
        else return {};
    }

    [[nodiscard]] Optional<float> tickInterval() const noexcept
    {
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<GlobalVarsTickIntervalFunction>()) {
            if (globalVars) {
                if (const auto function = hookContext.patternSearchResults().template get<GlobalVarsTickIntervalFunction>(); function) {
                    const auto value = function(globalVars);
                    if (Math::isFinite(value))
                        return value;
                }
            }
        }
        return {};
    }

    HookContext& hookContext;
    cs2::GlobalVars* globalVars;
};
