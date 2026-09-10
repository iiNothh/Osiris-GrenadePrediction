#pragma once

#include <CS2/Classes/Vector.h>
#include <GameClient/EngineTrace/TraceFilter.h>

namespace engine_trace {
    struct HullTraceRequest {
        cs2::Vector start{};
        cs2::Vector end{};
        cs2::Vector mins{};
        cs2::Vector maxs{};
        TraceFilterExcludedEntities excludedEntities{};
        TraceFilterParameters filter{};

        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return start.isFinite() && end.isFinite()
                && mins.isFinite() && maxs.isFinite()
                && mins.isLessThanOrEqualTo(maxs);
        }
    };
}
