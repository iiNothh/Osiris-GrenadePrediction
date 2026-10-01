#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <CS2/EngineTrace/CTraceFilter.h>
#include <GameClient/EngineTrace/GrenadeTraceParams.h>
#include <GameClient/EngineTrace/TraceConversion.h>
#include <GameClient/EngineTrace/HullTraceRequest.h>
#include <GameClient/EngineTrace/TraceLayout.h>
#include <GameClient/EngineTrace/TraceResult.h>
#include <MemoryPatterns/PatternTypes/EngineTracePatternTypes.h>
#include <Utils/ByteStorage.h>

namespace engine_trace::grenade {
    [[nodiscard]] inline bool applyFilterOverlayWithValidatedLayout(cs2::CTraceFilter& filter, const FilterOverlayLayout& layout,
        const TraceFilterParameters& parameters = {}) noexcept
    {
        if (parameters.interactsExclude.hasValue() != parameters.interactsAs.hasValue())
            return false;

        filter.writeValue(layout.interactsExcludeOffset, parameters.interactsExclude.valueOr(kFilterInteractionMask));
        filter.writeValue(layout.interactsAsOffset, parameters.interactsAs.valueOr(kFilterObjectMask));
        filter.storage[layout.flagsOffset] |= std::byte{0x02};
        filter.storage[layout.candidateCollectionModeOffset] = std::byte{0x01};
        return true;
    }

    [[nodiscard]] inline bool applyFilterOverlay(cs2::CTraceFilter& filter, const FilterOverlayLayout& layout,
        const TraceFilterParameters& parameters = {}) noexcept
    {
        return hasValidFilterOverlayLayout(layout) && applyFilterOverlayWithValidatedLayout(filter, layout, parameters);
    }

    struct TraceBindings {
        UnpackStrongTypeAliasT<TraceShapeFunctionPointer> traceShape{};
        UnpackStrongTypeAliasT<BuildRnQueryShapeAttrFromAABBFunctionPointer> buildQueryShape{};
        UnpackStrongTypeAliasT<PhysicsWorldPointerSlotStoragePointer> physicsWorldPointerSlotStorage{};
        UnpackStrongTypeAliasT<CTraceFilterConstructionFunctionPointer> constructFilter{};
        UnpackStrongTypeAliasT<CTraceFilterAddExcludedEntityFunctionPointer> addSecondExcludedEntity{};
        TraceOutputLayout outputLayout{};
        FilterOverlayLayout filterOverlayLayout{};
    };

    template <typename HookContext>
    [[nodiscard]] TraceBindings resolveBindings(HookContext& hookContext) noexcept
    {
        const auto& results = hookContext.patternSearchResults();
        return {
            .traceShape = results.template get<TraceShapeFunctionPointer>(),
            .buildQueryShape = results.template get<BuildRnQueryShapeAttrFromAABBFunctionPointer>(),
            .physicsWorldPointerSlotStorage = results.template get<PhysicsWorldPointerSlotStoragePointer>(),
            .constructFilter = results.template get<CTraceFilterConstructionFunctionPointer>(),
            .addSecondExcludedEntity = results.template get<CTraceFilterAddExcludedEntityFunctionPointer>(),
            .outputLayout = {
                .endPositionOffset = results.template get<CGameTraceEndPositionOffset>().value(),
                .normalOffset = results.template get<CGameTraceNormalOffset>().value(),
                .fractionOffset = results.template get<CGameTraceFractionOffset>().value(),
                .rawEntityHandleOffset = static_cast<std::int32_t>(results.template get<CGameTraceRawEntityHandleOffset>().value())
            },
            .filterOverlayLayout = {
                .interactsExcludeOffset = results.template get<CTraceFilterInteractsExcludeOffset>().value(),
                .interactsAsOffset = results.template get<CTraceFilterInteractsAsOffset>().value(),
                .flagsOffset = results.template get<CTraceFilterFlagsOffset>().value(),
                .candidateCollectionModeOffset = results.template get<CTraceFilterCandidateCollectionModeOffset>().value()
            }
        };
    }

    [[nodiscard]] inline bool hasValidImmutableBindings(const TraceBindings& bindings) noexcept
    {
        if (bindings.traceShape == nullptr || bindings.buildQueryShape == nullptr
            || bindings.physicsWorldPointerSlotStorage == nullptr
            || bindings.constructFilter == nullptr || bindings.addSecondExcludedEntity == nullptr
            || !hasValidTraceOutputLayout(bindings.outputLayout) || !bindings.outputLayout.rawEntityHandleOffset.hasValue()
            || !isValidCGameTraceRawEntityHandleOffset(bindings.outputLayout.rawEntityHandleOffset.value(),
                bindings.outputLayout.endPositionOffset, bindings.outputLayout.normalOffset, bindings.outputLayout.fractionOffset)
            || !hasValidFilterOverlayLayout(bindings.filterOverlayLayout))
            return false;
        return true;
    }

    [[nodiscard]] inline bool hasValidBindings(const TraceBindings& bindings) noexcept
    {
        return hasValidImmutableBindings(bindings) && *bindings.physicsWorldPointerSlotStorage != nullptr;
    }

    struct LaunchEndpointTraceBindings {
        UnpackStrongTypeAliasT<TraceShapeFunctionPointer> traceShape{};
        UnpackStrongTypeAliasT<BuildRnQueryShapeAttrFromAABBFunctionPointer> buildQueryShape{};
        UnpackStrongTypeAliasT<PhysicsWorldPointerSlotStoragePointer> physicsWorldPointerSlotStorage{};
        UnpackStrongTypeAliasT<CTraceFilterConstructionFunctionPointer> constructFilter{};
        std::int32_t endPositionOffset{};
    };

    template <typename HookContext>
    [[nodiscard]] LaunchEndpointTraceBindings resolveLaunchEndpointBindings(HookContext& hookContext) noexcept
    {
        const auto& results = hookContext.patternSearchResults();
        return {
            .traceShape = results.template get<TraceShapeFunctionPointer>(),
            .buildQueryShape = results.template get<BuildRnQueryShapeAttrFromAABBFunctionPointer>(),
            .physicsWorldPointerSlotStorage = results.template get<PhysicsWorldPointerSlotStoragePointer>(),
            .constructFilter = results.template get<CTraceFilterConstructionFunctionPointer>(),
            .endPositionOffset = results.template get<CGameTraceEndPositionOffset>().value()
        };
    }

    [[nodiscard]] inline bool hasValidLaunchEndpointBindings(const LaunchEndpointTraceBindings& bindings) noexcept
    {
        return bindings.traceShape != nullptr && bindings.buildQueryShape != nullptr
            && bindings.physicsWorldPointerSlotStorage != nullptr && *bindings.physicsWorldPointerSlotStorage != nullptr
            && bindings.constructFilter != nullptr
            && isValidCGameTraceOutputOffset(bindings.endPositionOffset, sizeof(cs2::Vector));
    }

    template <typename HookContext>
    [[nodiscard]] Optional<cs2::Vector> traceLaunchEndpoint(HookContext& hookContext, cs2::Vector start, cs2::Vector end, void* owner) noexcept
    {
        if (!start.isFinite() || !end.isFinite() || owner == nullptr)
            return {};

        const auto bindings = resolveLaunchEndpointBindings(hookContext);
        if (!hasValidLaunchEndpointBindings(bindings))
            return {};

        const HullTraceRequest request{
            .start = start,
            .end = end,
            .mins = {kHullMins.x - 0.02f, kHullMins.y - 0.02f, kHullMins.z - 0.02f},
            .maxs = {kHullMaxs.x + 0.02f, kHullMaxs.y + 0.02f, kHullMaxs.z + 0.02f}
        };
        const auto queryShape = makeQueryShape(bindings.buildQueryShape, request);
        cs2::CTraceFilter filter{};
        cs2::CGameTrace output{};
        if (bindings.constructFilter(&filter, owner, kLaunchEndpointFilter.interactsWith, kLaunchEndpointFilter.collisionGroup,
                kLaunchEndpointFilter.queryFlags) != &filter)
            return {};

        const auto endpointOffset = static_cast<std::size_t>(bindings.endPositionOffset);
        const float nonFinite = std::bit_cast<float>(std::uint32_t{0x7FC00000u});
        if (!byte_storage::write(output.storage, endpointOffset, cs2::Vector{nonFinite, nonFinite, nonFinite}))
            return {};

        bindings.traceShape(*bindings.physicsWorldPointerSlotStorage, &queryShape, &start, &end, &filter, &output);
        const auto endpoint = output.readValue<cs2::Vector>(endpointOffset);
        return endpoint.isFinite() ? Optional<cs2::Vector>{endpoint} : Optional<cs2::Vector>{};
    }

    template <typename HookContext>
    [[nodiscard]] bool isTraceAvailable(HookContext& hookContext) noexcept
    {
        const auto bindings = resolveBindings(hookContext);
        return hasValidBindings(bindings);
    }

    [[nodiscard]] inline Optional<TraceResult> traceHull(const TraceBindings& bindings, const HullTraceRequest& request) noexcept
    {
        if (!request.isValid() || bindings.physicsWorldPointerSlotStorage == nullptr || *bindings.physicsWorldPointerSlotStorage == nullptr)
            return {};

        const auto queryShape = makeQueryShape(bindings.buildQueryShape, request);
        cs2::CTraceFilter filter{};
        cs2::CGameTrace output{};
        if (bindings.constructFilter(&filter, request.excludedEntities.first, request.filter.interactsWith, request.filter.collisionGroup, request.filter.queryFlags)
                != &filter)
            return {};

        if (!applyFilterOverlayWithValidatedLayout(filter, bindings.filterOverlayLayout, request.filter))
            return {};
        if (request.excludedEntities.second != nullptr)
            bindings.addSecondExcludedEntity(&filter, request.excludedEntities.first, request.excludedEntities.second);

        const auto physicsWorldPointerSlot = *bindings.physicsWorldPointerSlotStorage;
        if (physicsWorldPointerSlot == nullptr)
            return {};
        bindings.traceShape(physicsWorldPointerSlot, &queryShape, &request.start, &request.end, &filter, &output);
        return decodeValidatedTraceOutput(output, bindings.outputLayout);
    }

    template <typename HookContext>
    [[nodiscard]] Optional<TraceResult> traceHull(HookContext& hookContext, const HullTraceRequest& request) noexcept
    {
        if (!request.isValid())
            return {};

        const auto bindings = resolveBindings(hookContext);
        if (!hasValidBindings(bindings))
            return {};
        return traceHull(bindings, request);
    }
}
