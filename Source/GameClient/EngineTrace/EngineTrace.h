#pragma once

#include <cstdint>

#include <GameClient/EngineTrace/TraceConversion.h>
#include <GameClient/EngineTrace/HullTraceRequest.h>
#include <GameClient/EngineTrace/GrenadeTrace.h>
#include <GameClient/EngineTrace/TraceResult.h>
#include <MemoryPatterns/PatternTypes/EngineTracePatternTypes.h>
#include <Utils/Optional.h>

template <typename HookContext>
class EngineTrace {
public:
    explicit EngineTrace(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    [[nodiscard]] Optional<TraceResult> traceHull(const engine_trace::HullTraceRequest& request) const noexcept
    {
        if (!request.isValid())
            return {};

        const auto bindings = resolveBindings();
        if (!hasValidBindings(bindings, request.excludedEntities.second != nullptr))
            return {};

        const auto queryShape = engine_trace::makeQueryShape(bindings.buildQueryShape, request);
        cs2::CTraceFilter filter{};
        cs2::CGameTrace output{};

        if (!initializeFilter(bindings, request, filter))
            return {};

        const auto physicsWorldPointerSlot = *bindings.physicsWorldPointerSlotStorage;
        if (!physicsWorldPointerSlot)
            return {};

        bindings.traceShape(physicsWorldPointerSlot, &queryShape, &request.start, &request.end, &filter, &output);
        return engine_trace::decodeTraceOutput(output, bindings.outputLayout);
    }

    [[nodiscard]] bool isGrenadeHullTraceAvailable() const noexcept
    {
        return engine_trace::grenade::isTraceAvailable(hookContext);
    }

    [[nodiscard]] Optional<TraceResult> traceGrenadeHull(const engine_trace::HullTraceRequest& request) const noexcept
    {
        return engine_trace::grenade::traceHull(hookContext, request);
    }

private:
    struct TraceBindings {
        UnpackStrongTypeAliasT<TraceShapeFunctionPointer> traceShape{};
        UnpackStrongTypeAliasT<BuildRnQueryShapeAttrFromAABBFunctionPointer> buildQueryShape{};
        UnpackStrongTypeAliasT<PhysicsWorldPointerSlotStoragePointer> physicsWorldPointerSlotStorage{};
        UnpackStrongTypeAliasT<CTraceFilterConstructionFunctionPointer> constructFilter{};
        UnpackStrongTypeAliasT<CTraceFilterAddExcludedEntityFunctionPointer> addSecondExcludedEntity{};
        engine_trace::TraceOutputLayout outputLayout{};
    };

    [[nodiscard]] TraceBindings resolveBindings() const noexcept
    {
        const auto& results = hookContext.patternSearchResults();
        TraceBindings bindings{
            .traceShape = results.template get<TraceShapeFunctionPointer>(),
            .buildQueryShape = results.template get<BuildRnQueryShapeAttrFromAABBFunctionPointer>(),
            .physicsWorldPointerSlotStorage = results.template get<PhysicsWorldPointerSlotStoragePointer>(),
            .constructFilter = results.template get<CTraceFilterConstructionFunctionPointer>(),
            .outputLayout = {
                .endPositionOffset = results.template get<CGameTraceEndPositionOffset>().value(),
                .normalOffset = results.template get<CGameTraceNormalOffset>().value(),
                .fractionOffset = results.template get<CGameTraceFractionOffset>().value()
            }
        };
        bindings.addSecondExcludedEntity = results.template get<CTraceFilterAddExcludedEntityFunctionPointer>();
        bindings.outputLayout.rawEntityHandleOffset = static_cast<std::int32_t>(results.template get<CGameTraceRawEntityHandleOffset>().value());
        return bindings;
    }

    [[nodiscard]] static bool hasValidBindings(const TraceBindings& bindings, bool needsSecondExclusion) noexcept
    {
        return bindings.traceShape != nullptr && bindings.buildQueryShape != nullptr
            && bindings.physicsWorldPointerSlotStorage != nullptr && *bindings.physicsWorldPointerSlotStorage != nullptr
            && bindings.constructFilter != nullptr && (!needsSecondExclusion || bindings.addSecondExcludedEntity != nullptr)
            && engine_trace::hasValidTraceOutputLayout(bindings.outputLayout);
    }

    [[nodiscard]] static bool initializeFilter(const TraceBindings& bindings, const engine_trace::HullTraceRequest& request, cs2::CTraceFilter& filter) noexcept
    {
        if (bindings.constructFilter(&filter, request.excludedEntities.first, request.filter.interactsWith, request.filter.collisionGroup, request.filter.queryFlags) != &filter)
            return false;

        if (request.excludedEntities.second != nullptr)
            bindings.addSecondExcludedEntity(&filter, request.excludedEntities.first, request.excludedEntities.second);
        return true;
    }

    HookContext& hookContext;
};
