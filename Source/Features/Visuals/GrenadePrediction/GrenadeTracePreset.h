#pragma once

#include <CS2/Constants/CollisionGroup.h>
#include <CS2/Constants/InteractionLayers.h>
#include <CS2/Constants/PhysicsQueryFlag.h>
#include <GameClient/EngineTrace/HullTraceRequest.h>
#include <GameClient/EngineTrace/TraceFilter.h>
#include <GameClient/EngineTrace/TraceResult.h>

namespace grenade_trace_preset {

    constexpr cs2::Vector kHullMins{-2.0f, -2.0f, -2.0f};
    constexpr cs2::Vector kHullMaxs{2.0f, 2.0f, 2.0f};
    constexpr auto kGrenadeInteractionLayerMask = (cs2::engine_trace::InteractionLayer::Solid | cs2::engine_trace::InteractionLayer::Hitboxes
        | cs2::engine_trace::InteractionLayer::Sky | cs2::engine_trace::InteractionLayer::Window | cs2::engine_trace::InteractionLayer::PassBullets
        | cs2::engine_trace::InteractionLayer::Player | cs2::engine_trace::InteractionLayer::Npc | cs2::engine_trace::InteractionLayer::Debris)
        & ~cs2::engine_trace::InteractionLayer::Window;

    constexpr engine_trace::TraceFilterParameters kFilter{
        .interactsWith = kGrenadeInteractionLayerMask,
        .collisionGroup = cs2::CollisionGroup::Default,
        .queryFlags = cs2::PhysicsQueryFlag::IncludeSolidContacts
            | cs2::PhysicsQueryFlag::RespectDisabledSolidContacts | cs2::PhysicsQueryFlag::IncludeTriggerContacts
    };

    [[nodiscard]] constexpr engine_trace::HullTraceRequest makeRequest(cs2::Vector start, cs2::Vector end, engine_trace::TraceFilterExcludedEntities excludedEntities = {}) noexcept
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
        return trace.isGrenadeHullTraceAvailable();
    }

    template <typename EngineTrace>
    [[nodiscard]] Optional<TraceResult> traceInFlightHull(EngineTrace&& trace, cs2::Vector start, cs2::Vector end, engine_trace::TraceFilterExcludedEntities excludedEntities) noexcept
    {
        return trace.traceGrenadeHull(makeRequest(start, end, excludedEntities));
    }
}
