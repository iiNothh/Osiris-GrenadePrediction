#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <type_traits>

#include <GameClient/EngineTrace/EngineTraceTypes.h>
#include <MemoryPatterns/PatternTypes/EngineTracePatternTypes.h>
#include <Platform/Macros/IsPlatform.h>
#include <Utils/Optional.h>

struct TraceResult {
    float fraction{};
    cs2::Vector endPos{};
    cs2::Vector normal{};
    std::int32_t rawEntityHandle{};
    bool handleRead{};
};

namespace engine_trace
{
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

#if IS_WIN64()
    template <typename PatternSearchResults>
    inline constexpr bool hasNativePipTracePatternSupport = PatternSearchResults::template supports<TraceShapeFunctionPointer>()
        && PatternSearchResults::template supports<GameTraceManagerStoragePointer>()
        && PatternSearchResults::template supports<InitFilterFunctionPointer>()
        && PatternSearchResults::template supports<AddSecondExcludedEntityToFilterFunctionPointer>()
        && PatternSearchResults::template supports<CGameTraceEndPositionOffset>()
        && PatternSearchResults::template supports<CGameTraceNormalOffset>()
        && PatternSearchResults::template supports<CGameTraceFractionOffset>()
        && PatternSearchResults::template supports<CGameTraceRawEntityHandleOffset>()
        && PatternSearchResults::template supports<CTraceFilterBaseVtablePointer>()
        && PatternSearchResults::template supports<NativePipTraceFilterBodyProfilePointer>()
        && PatternSearchResults::template supports<NativePipPushFilterConstructorPointer>()
        && PatternSearchResults::template supports<PrimaryTraceShapeCandidateProfileMarker>()
        && PatternSearchResults::template supports<SecondaryTraceShapeCandidateProfileMarker>()
        && PatternSearchResults::template supports<PrimaryToSecondaryEnumerationCallsiteMarker>()
        && PatternSearchResults::template supports<SecondaryCandidateEnumerationFunctionPointer>();
#endif
}

template <typename HookContext>
class EngineTrace {
public:
    explicit EngineTrace(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    [[nodiscard]] Optional<TraceResult> traceGrenadeHull(cs2::Vector start, cs2::Vector end, void* skipEntity) const noexcept
    {
        return traceGrenadeHull(start, end, engine_trace::TraceFilterExcludedEntities{skipEntity, nullptr}, engine_trace::kMaskGrenade, 4, 7);
    }

    [[nodiscard]] Optional<TraceResult> traceGrenadeHull(
        cs2::Vector start, cs2::Vector end, void* skipEntity,
        std::uint64_t mask, std::uint8_t collisionGroup, std::uint8_t queryByte) const noexcept
    {
        return traceGrenadeHull(start, end, engine_trace::TraceFilterExcludedEntities{skipEntity, nullptr}, mask, collisionGroup, queryByte);
    }

    [[nodiscard]] Optional<TraceResult> traceGrenadeHull(
        cs2::Vector start, cs2::Vector end, engine_trace::TraceFilterExcludedEntities excludedEntities,
        std::uint64_t mask, std::uint8_t collisionGroup, std::uint8_t queryByte) const noexcept
    {
        if (!engine_trace::isFinite(start) || !engine_trace::isFinite(end))
            return {};

        using PatternSearchResults = std::remove_cvref_t<decltype(hookContext.patternSearchResults())>;
        if constexpr (PatternSearchResults::template supports<TraceShapeFunctionPointer>()
            && PatternSearchResults::template supports<GameTraceManagerStoragePointer>()
            && PatternSearchResults::template supports<InitFilterFunctionPointer>()
            && PatternSearchResults::template supports<CGameTraceEndPositionOffset>()
            && PatternSearchResults::template supports<CGameTraceNormalOffset>()
            && PatternSearchResults::template supports<CGameTraceFractionOffset>()) {
            const auto& results = hookContext.patternSearchResults();
            const auto traceShape = results.template get<TraceShapeFunctionPointer>();
            const auto managerStorage = results.template get<GameTraceManagerStoragePointer>();
            const auto initFilter = results.template get<InitFilterFunctionPointer>();
            const auto endPositionOffset = results.template get<CGameTraceEndPositionOffset>();
            const auto normalOffset = results.template get<CGameTraceNormalOffset>();
            const auto fractionOffset = results.template get<CGameTraceFractionOffset>();

            if (traceShape == nullptr || managerStorage == nullptr || initFilter == nullptr
                || !engine_trace::areValidOutputOffsets(endPositionOffset, normalOffset, fractionOffset))
                return {};

            void* const managerHolder = *managerStorage;
            if (managerHolder == nullptr)
                return {};

            if (excludedEntities.second != nullptr) {
                if constexpr (PatternSearchResults::template supports<AddSecondExcludedEntityToFilterFunctionPointer>()) {
                    const auto addSecondExcludedEntity = results.template get<AddSecondExcludedEntityToFilterFunctionPointer>();
                    if (addSecondExcludedEntity == nullptr)
                        return {};
                    return trace(start, end, excludedEntities, mask, collisionGroup, queryByte, managerHolder, traceShape, initFilter, addSecondExcludedEntity);
                } else {
                    return {};
                }
            }
            return trace(start, end, excludedEntities, mask, collisionGroup, queryByte, managerHolder, traceShape, initFilter, nullptr);
        } else return {};
    }

    [[nodiscard]] bool isInFlightGrenadeTraceAvailable() const noexcept
    {
#if IS_WIN64()
        using PatternSearchResults = std::remove_cvref_t<decltype(hookContext.patternSearchResults())>;
        if constexpr (engine_trace::hasNativePipTracePatternSupport<PatternSearchResults>) {
            const auto& results = hookContext.patternSearchResults();
            const auto traceShape = results.template get<TraceShapeFunctionPointer>();
            const auto managerStorage = results.template get<GameTraceManagerStoragePointer>();
            const auto initFilter = results.template get<InitFilterFunctionPointer>();
            const auto addSecondExcludedEntity = results.template get<AddSecondExcludedEntityToFilterFunctionPointer>();
            const auto endPositionOffset = results.template get<CGameTraceEndPositionOffset>();
            const auto normalOffset = results.template get<CGameTraceNormalOffset>();
            const auto fractionOffset = results.template get<CGameTraceFractionOffset>();
            const auto rawEntityHandleOffset = results.template get<CGameTraceRawEntityHandleOffset>();
            const auto baseVtable = results.template get<CTraceFilterBaseVtablePointer>();
            const auto pipFilterBody = results.template get<NativePipTraceFilterBodyProfilePointer>();
            const auto pushFilterConstructor = results.template get<NativePipPushFilterConstructorPointer>();
            const auto primaryTraceShapeCandidate = results.template get<PrimaryTraceShapeCandidateProfileMarker>();
            const auto secondaryTraceShapeCandidate = results.template get<SecondaryTraceShapeCandidateProfileMarker>();
            const auto primaryToSecondaryCallsite = results.template get<PrimaryToSecondaryEnumerationCallsiteMarker>();
            const auto secondaryCandidateEnumerationFunction = results.template get<SecondaryCandidateEnumerationFunctionPointer>();

            if (traceShape == nullptr || managerStorage == nullptr || initFilter == nullptr || addSecondExcludedEntity == nullptr
                || !engine_trace::hasNativePipProfileMarkers(baseVtable, pipFilterBody, pushFilterConstructor,
                    primaryTraceShapeCandidate, secondaryTraceShapeCandidate, primaryToSecondaryCallsite, secondaryCandidateEnumerationFunction)
                || !engine_trace::areValidOutputOffsets(endPositionOffset, normalOffset, fractionOffset)
                || !engine_trace::isValidRawEntityHandleOffset(rawEntityHandleOffset, endPositionOffset, normalOffset, fractionOffset))
                return false;

            void* const managerHolder = *managerStorage;
            const auto* const callback = engine_trace::nativePipBaseFilterCallback(hookContext.clientVmtSection(), baseVtable);
            if (managerHolder == nullptr || !engine_trace::hasExpectedNativePipBaseFilterCallback(hookContext.clientCodeSection(), callback))
                return false;

            return engine_trace::hasCurrentNativePipProfileStructure(
                reinterpret_cast<std::uintptr_t>(primaryTraceShapeCandidate), reinterpret_cast<std::uintptr_t>(secondaryTraceShapeCandidate),
                reinterpret_cast<std::uintptr_t>(primaryToSecondaryCallsite), reinterpret_cast<std::uintptr_t>(secondaryCandidateEnumerationFunction));
        } else return false;
#else
        return false;
#endif
    }

    [[nodiscard]] Optional<TraceResult> traceInFlightGrenadeHull(
        cs2::Vector start, cs2::Vector end, engine_trace::TraceFilterExcludedEntities excludedEntities) const noexcept
    {
#if IS_WIN64()
        if (!engine_trace::isFinite(start) || !engine_trace::isFinite(end))
            return {};

        using PatternSearchResults = std::remove_cvref_t<decltype(hookContext.patternSearchResults())>;
        if constexpr (!engine_trace::hasNativePipTracePatternSupport<PatternSearchResults>) {
            return {};
        } else {
            if (!isInFlightGrenadeTraceAvailable())
                return {};

            const auto& results = hookContext.patternSearchResults();
            const auto traceShape = results.template get<TraceShapeFunctionPointer>();
            const auto managerStorage = results.template get<GameTraceManagerStoragePointer>();
            const auto initFilter = results.template get<InitFilterFunctionPointer>();
            const auto addSecondExcludedEntity = results.template get<AddSecondExcludedEntityToFilterFunctionPointer>();
            const auto endPositionOffset = results.template get<CGameTraceEndPositionOffset>();
            const auto normalOffset = results.template get<CGameTraceNormalOffset>();
            const auto fractionOffset = results.template get<CGameTraceFractionOffset>();
            const auto rawEntityHandleOffset = results.template get<CGameTraceRawEntityHandleOffset>();
            const auto baseVtable = results.template get<CTraceFilterBaseVtablePointer>();
            void* const managerHolder = *managerStorage;

            engine_trace::FixedGrenadeHullTraceDescriptor descriptor{};
            engine_trace::FilterStorage filter{};
            engine_trace::OutputStorage output{};
            if (initFilter(&filter, excludedEntities.first, engine_trace::kNativePipFirstInteraction,
                    engine_trace::kNativePipCollisionGroup, engine_trace::kNativePipQueryByte) != static_cast<void*>(&filter)
                || !engine_trace::hasExpectedNativePipFilterInitialization(filter, baseVtable))
                return {};

            engine_trace::applyNativePipFilterOverlay(filter);
            if (excludedEntities.second != nullptr)
                addSecondExcludedEntity(&filter, excludedEntities.first, excludedEntities.second);
            traceShape(managerHolder, &descriptor, &start, &end, &filter, &output);

            const auto fraction = readOutput<float>(output, fractionOffset);
            const auto endPosition = readOutput<cs2::Vector>(output, endPositionOffset);
            const auto normal = readOutput<cs2::Vector>(output, normalOffset);
            if (!engine_trace::isFinite(fraction) || fraction < 0.0f || fraction > 1.0f
                || !engine_trace::isFinite(endPosition) || !engine_trace::isFinite(normal)
                || (fraction < 1.0f && !engine_trace::hasUsableNormal(normal)))
                return {};

            const bool handleRead = fraction < 1.0f;
            const auto rawEntityHandle = handleRead ? readOutput<std::int32_t>(output, rawEntityHandleOffset) : std::int32_t{};
            return TraceResult{fraction, endPosition, normal, rawEntityHandle, handleRead};
        }
#else
        static_cast<void>(start);
        static_cast<void>(end);
        static_cast<void>(excludedEntities);
        return {};
#endif
    }

private:
    [[nodiscard]] Optional<TraceResult> trace(
        cs2::Vector start, cs2::Vector end, engine_trace::TraceFilterExcludedEntities excludedEntities,
        std::uint64_t mask, std::uint8_t collisionGroup, std::uint8_t queryByte, void* managerHolder,
        UnpackStrongTypeAliasT<TraceShapeFunctionPointer> traceShape,
        UnpackStrongTypeAliasT<InitFilterFunctionPointer> initFilter,
        UnpackStrongTypeAliasT<AddSecondExcludedEntityToFilterFunctionPointer> addSecondExcludedEntity) const noexcept
    {
        engine_trace::FixedGrenadeHullTraceDescriptor descriptor{};
        engine_trace::FilterStorage filter{};
        engine_trace::OutputStorage output{};

        initFilter(&filter, excludedEntities.first, mask, collisionGroup, queryByte);
        if (excludedEntities.second != nullptr)
            addSecondExcludedEntity(&filter, excludedEntities.first, excludedEntities.second);
        traceShape(managerHolder, &descriptor, &start, &end, &filter, &output);

        const auto fractionOffset = hookContext.patternSearchResults().template get<CGameTraceFractionOffset>();
        const auto endPositionOffset = hookContext.patternSearchResults().template get<CGameTraceEndPositionOffset>();
        const auto normalOffset = hookContext.patternSearchResults().template get<CGameTraceNormalOffset>();
        const auto fraction = readOutput<float>(output, fractionOffset);
        const auto endPosition = readOutput<cs2::Vector>(output, endPositionOffset);
        const auto normal = readOutput<cs2::Vector>(output, normalOffset);

        if (!engine_trace::isFinite(fraction) || fraction < 0.0f || fraction > 1.0f
            || !engine_trace::isFinite(endPosition) || !engine_trace::isFinite(normal)
            || (fraction < 1.0f && !engine_trace::hasUsableNormal(normal)))
            return {};

        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<CGameTraceRawEntityHandleOffset>()) {
            const auto rawEntityHandleOffset = hookContext.patternSearchResults().template get<CGameTraceRawEntityHandleOffset>();
            const bool handleRead = fraction < 1.0f && engine_trace::isValidRawEntityHandleOffset(
                rawEntityHandleOffset, endPositionOffset, normalOffset, fractionOffset);
            const auto rawEntityHandle = handleRead ? readOutput<std::int32_t>(output, rawEntityHandleOffset) : std::int32_t{};
            return TraceResult{fraction, endPosition, normal, rawEntityHandle, handleRead};
        } else return TraceResult{fraction, endPosition, normal};
    }

    template <typename T>
    [[nodiscard]] static T readOutput(const engine_trace::OutputStorage& output, std::int32_t offset) noexcept
    {
        std::array<std::byte, sizeof(T)> bytes{};
        for (std::size_t i = 0; i < sizeof(T); ++i)
            bytes[i] = output.storage[static_cast<std::size_t>(offset) + i];
        return std::bit_cast<T>(bytes);
    }

    HookContext& hookContext;
};
