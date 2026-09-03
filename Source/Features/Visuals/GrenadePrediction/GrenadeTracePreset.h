#pragma once

#include <cstdint>

#include <GameClient/EngineTrace/EngineTrace.h>

namespace grenade_trace_preset {

constexpr cs2::Vector kHullMins{-2.0f, -2.0f, -2.0f};
constexpr cs2::Vector kHullMaxs{2.0f, 2.0f, 2.0f};
constexpr engine_trace::TraceFilterParameters kFilter{
    .mask = engine_trace::kMaskGrenade,
    .collisionGroup = 4,
    .queryByte = 7
};

[[nodiscard]] constexpr engine_trace::HullTraceRequest makeRequest(cs2::Vector start, cs2::Vector end,
    engine_trace::TraceFilterExcludedEntities excludedEntities = {}) noexcept
{
    return {
        .start = start,
        .end = end,
        .mins = kHullMins,
        .maxs = kHullMaxs,
        .excludedEntities = excludedEntities,
        .filter = kFilter
    };
}

template <typename EngineTrace>
[[nodiscard]] Optional<TraceResult> traceSpawnHull(EngineTrace&& trace, cs2::Vector start, cs2::Vector end, void* skipEntity) noexcept
{
    return trace.traceHull(makeRequest(start, end, {skipEntity}));
}

template <typename EngineTrace>
[[nodiscard]] bool isInFlightTraceAvailable(EngineTrace&& trace) noexcept
{
    return trace.isNativePipHullTraceAvailable();
}

template <typename EngineTrace>
[[nodiscard]] Optional<TraceResult> traceInFlightHull(EngineTrace&& trace, cs2::Vector start, cs2::Vector end,
    engine_trace::TraceFilterExcludedEntities excludedEntities) noexcept
{
    return trace.traceNativePipHull(makeRequest(start, end, excludedEntities));
}

}
