#pragma once

#include <cstddef>
#include <cstdint>
#include <CS2/EngineTrace/CTraceFilter.h>
#include <GameClient/EngineTrace/GrenadeTraceParams.h>
#include <GameClient/EngineTrace/TraceConversion.h>
#include <GameClient/EngineTrace/HullTraceRequest.h>
#include <GameClient/EngineTrace/TraceLayout.h>
#include <GameClient/EngineTrace/TraceResult.h>
#include <MemoryPatterns/PatternTypes/EngineTracePatternTypes.h>

namespace engine_trace::grenade {
    [[nodiscard]] inline bool applyFilterOverlay(cs2::CTraceFilter& filter, const FilterOverlayLayout& layout) noexcept
    {
        if (!hasValidFilterOverlayLayout(layout))
            return false;

        filter.writeValue(layout.interactsExcludeOffset, kFilterInteractionMask);
        filter.writeValue(layout.interactsAsOffset, kFilterObjectMask);
        filter.storage[layout.flagsOffset] |= std::byte{0x02};
        filter.storage[layout.candidateCollectionModeOffset] = std::byte{0x01};
        return true;
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

    [[nodiscard]] inline bool hasValidBindings(const TraceBindings& bindings) noexcept
    {
        if (bindings.traceShape == nullptr || bindings.buildQueryShape == nullptr
            || bindings.physicsWorldPointerSlotStorage == nullptr || *bindings.physicsWorldPointerSlotStorage == nullptr
            || bindings.constructFilter == nullptr || bindings.addSecondExcludedEntity == nullptr
            || !hasValidTraceOutputLayout(bindings.outputLayout) || !bindings.outputLayout.rawEntityHandleOffset.hasValue()
            || !isValidCGameTraceRawEntityHandleOffset(bindings.outputLayout.rawEntityHandleOffset.value(),
                bindings.outputLayout.endPositionOffset, bindings.outputLayout.normalOffset, bindings.outputLayout.fractionOffset)
            || !hasValidFilterOverlayLayout(bindings.filterOverlayLayout))
            return false;
        return true;
    }

    template <typename HookContext>
    [[nodiscard]] bool isTraceAvailable(HookContext& hookContext) noexcept
    {
        const auto bindings = resolveBindings(hookContext);
        return hasValidBindings(bindings);
    }

    template <typename HookContext>
    [[nodiscard]] Optional<TraceResult> traceHull(HookContext& hookContext, const HullTraceRequest& request) noexcept
    {
        if (!request.isValid())
            return {};

        const auto bindings = resolveBindings(hookContext);
        if (!hasValidBindings(bindings))
            return {};

        const auto queryShape = makeQueryShape(bindings.buildQueryShape, request);
        cs2::CTraceFilter filter{};
        cs2::CGameTrace output{};
        if (bindings.constructFilter(&filter, request.excludedEntities.first, kFirstInteraction, kCollisionGroup, kQueryFlags)
                != &filter)
            return {};

        if (!applyFilterOverlay(filter, bindings.filterOverlayLayout))
            return {};
        if (request.excludedEntities.second != nullptr)
            bindings.addSecondExcludedEntity(&filter, request.excludedEntities.first, request.excludedEntities.second);

        const auto physicsWorldPointerSlot = *bindings.physicsWorldPointerSlotStorage;
        if (physicsWorldPointerSlot == nullptr)
            return {};
        bindings.traceShape(physicsWorldPointerSlot, &queryShape, &request.start, &request.end, &filter, &output);
        return decodeTraceOutput(output, bindings.outputLayout);
    }
}
