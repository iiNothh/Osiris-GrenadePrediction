#pragma once

#include <cstdint>
#include <type_traits>

#include <GameClient/EngineTrace/EngineTraceTypes.h>
#include <GameClient/EngineTrace/NativePipTrace.h>
#include <MemoryPatterns/PatternTypes/EngineTracePatternTypes.h>
#include <Utils/Optional.h>

namespace engine_trace {

constexpr std::uint64_t kContentsSolid = 0x1;
constexpr std::uint64_t kContentsHitboxes = 0x2;
constexpr std::uint64_t kContentsSky = 0x8;
constexpr std::uint64_t kContentsWindow = 0x1000;
constexpr std::uint64_t kContentsPassBullets = 0x2000;
constexpr std::uint64_t kContentsPlayer = 0x40000;
constexpr std::uint64_t kContentsNpc = 0x80000;
constexpr std::uint64_t kContentsDebris = 0x100000;
constexpr std::uint64_t kMaskShot = kContentsSolid | kContentsHitboxes | kContentsWindow | kContentsPassBullets | kContentsPlayer | kContentsNpc | kContentsDebris;
constexpr std::uint64_t kMaskGrenade = (kMaskShot & ~kContentsWindow) | kContentsSky;

}

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

        using PatternSearchResults = std::remove_cvref_t<decltype(hookContext.patternSearchResults())>;
        if constexpr (!hasTracePatternSupport<PatternSearchResults>) {
            return {};
        } else {
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

    template <typename PatternSearchResults>
    inline static constexpr bool hasTracePatternSupport = PatternSearchResults::template supports<TraceShapeFunctionPointer>()
        && PatternSearchResults::template supports<GameTraceManagerStoragePointer>()
        && PatternSearchResults::template supports<InitFilterFunctionPointer>()
        && PatternSearchResults::template supports<CGameTraceEndPositionOffset>()
        && PatternSearchResults::template supports<CGameTraceNormalOffset>()
        && PatternSearchResults::template supports<CGameTraceFractionOffset>();

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
        using PatternSearchResults = std::remove_cvref_t<decltype(results)>;
        if constexpr (PatternSearchResults::template supports<AddSecondExcludedEntityToFilterFunctionPointer>())
            bindings.addSecondExcludedEntity = results.template get<AddSecondExcludedEntityToFilterFunctionPointer>();
        if constexpr (PatternSearchResults::template supports<CGameTraceRawEntityHandleOffset>())
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
