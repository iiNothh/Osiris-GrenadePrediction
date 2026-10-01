#pragma once

#include <GameClient/EngineTrace/GrenadeTraceParams.h>
#include <GameClient/EngineTrace/HullTraceRequest.h>
#include <GameClient/EngineTrace/TraceFilter.h>
#include <GameClient/EngineTrace/TraceResult.h>

namespace grenade_trace_preset {
    [[nodiscard]] constexpr engine_trace::HullTraceRequest makeRequest(cs2::Vector start, cs2::Vector end,
        engine_trace::TraceFilterExcludedEntities excludedEntities = {}, engine_trace::TraceFilterParameters filter = engine_trace::grenade::kInFlightFilter) noexcept
    {
        return {
            .start = start,
            .end = end,
            .mins = engine_trace::grenade::kHullMins,
            .maxs = engine_trace::grenade::kHullMaxs,
            .excludedEntities = excludedEntities,
            .filter = filter
        };
    }

    template <typename EngineTrace>
    [[nodiscard]] Optional<TraceResult> traceSpawnHull(EngineTrace&& trace, cs2::Vector start, cs2::Vector end, void* skipEntity) noexcept
    {
        return trace.traceHull(makeRequest(start, end, {skipEntity}, engine_trace::grenade::kSpawnFilter));
    }

    template <typename EngineTrace>
    [[nodiscard]] bool isInFlightTraceAvailable(EngineTrace&& trace) noexcept
    {
        return trace.isGrenadeHullTraceAvailable();
    }

    template <typename EngineTrace>
    [[nodiscard]] Optional<TraceResult> traceInFlightHull(EngineTrace&& trace, const auto& bindings, cs2::Vector start, cs2::Vector end,
        engine_trace::TraceFilterExcludedEntities excludedEntities, engine_trace::TraceFilterParameters filter = engine_trace::grenade::kInFlightFilter) noexcept
    {
        return trace.traceGrenadeHull(bindings, makeRequest(start, end, excludedEntities, filter));
    }
}
