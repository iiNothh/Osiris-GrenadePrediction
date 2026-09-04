#pragma once

#include <GameClient/EngineTrace/EngineTraceTypes.h>
#include <GameClient/EngineTrace/NativePipTrace.h>
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
        if (!engine_trace::isValidHullTraceRequest(request))
            return {};

        const auto bindings = resolveBindings();
        if (!hasValidBindings(bindings, request.excludedEntities.second != nullptr))
            return {};

        const auto descriptor = engine_trace::makeHullTraceDescriptor(request);
        cs2::engine_trace::TraceFilterStorage filter{};
        cs2::engine_trace::TraceOutputStorage output{};
        if (bindings.initFilter(&filter, request.excludedEntities.first, request.filter.mask, request.filter.collisionGroup, request.filter.queryByte)
            != static_cast<void*>(&filter))
            return {};

        if (request.excludedEntities.second != nullptr)
            bindings.addSecondExcludedEntity(&filter, request.excludedEntities.first, request.excludedEntities.second);

        void* const managerHolder = *bindings.managerStorage;
        if (managerHolder == nullptr)
            return {};
        bindings.traceShape(managerHolder, &descriptor, &request.start, &request.end, &filter, &output);
        return engine_trace::decodeTraceOutput(output, bindings.outputLayout);
    }

    [[nodiscard]] bool isNativePipHullTraceAvailable() const noexcept
    {
        return engine_trace::native_pip::isTraceAvailable(hookContext);
    }

    [[nodiscard]] Optional<TraceResult> traceNativePipHull(const engine_trace::HullTraceRequest& request) const noexcept
    {
        return engine_trace::native_pip::traceHull(hookContext, request);
    }

private:
    struct TraceBindings {
        UnpackStrongTypeAliasT<TraceShapeFunctionPointer> traceShape{};
        void** managerStorage{};
        UnpackStrongTypeAliasT<InitFilterFunctionPointer> initFilter{};
        UnpackStrongTypeAliasT<AddSecondExcludedEntityToFilterFunctionPointer> addSecondExcludedEntity{};
        engine_trace::TraceOutputLayout outputLayout{};
    };

    [[nodiscard]] TraceBindings resolveBindings() const noexcept
    {
        const auto& results = hookContext.patternSearchResults();
        TraceBindings bindings{
            .traceShape = results.template get<TraceShapeFunctionPointer>(),
            .managerStorage = results.template get<GameTraceManagerStoragePointer>(),
            .initFilter = results.template get<InitFilterFunctionPointer>(),
            .outputLayout = {
                .endPositionOffset = results.template get<CGameTraceEndPositionOffset>(),
                .normalOffset = results.template get<CGameTraceNormalOffset>(),
                .fractionOffset = results.template get<CGameTraceFractionOffset>()
            }
        };
        bindings.addSecondExcludedEntity = results.template get<AddSecondExcludedEntityToFilterFunctionPointer>();
        bindings.outputLayout.rawEntityHandleOffset = results.template get<CGameTraceRawEntityHandleOffset>();
        return bindings;
    }

    [[nodiscard]] static bool hasValidBindings(const TraceBindings& bindings, bool needsSecondExclusion) noexcept
    {
        return bindings.traceShape != nullptr && bindings.managerStorage != nullptr && *bindings.managerStorage != nullptr
            && bindings.initFilter != nullptr && (!needsSecondExclusion || bindings.addSecondExcludedEntity != nullptr)
            && engine_trace::hasValidTraceOutputLayout(bindings.outputLayout);
    }

    HookContext& hookContext;
};
