#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <type_traits>

#include <GameClient/EngineTrace/EngineTraceTypes.h>
#include <MemoryPatterns/PatternTypes/EngineTracePatternTypes.h>
#include <Utils/Optional.h>

struct TraceResult {
    float fraction;
    cs2::Vector endPos;
    cs2::Vector normal;
    bool floorDampingKnownEligible{};
    std::int32_t rawEntityHandle{};
    bool handleRead{};
};

constexpr std::uint64_t CONTENTS_SOLID = 0x1;
constexpr std::uint64_t CONTENTS_HITBOXES = 0x2;
constexpr std::uint64_t CONTENTS_SKY = 0x8;
constexpr std::uint64_t CONTENTS_WINDOW = 0x1000;
constexpr std::uint64_t CONTENTS_PASSBULLETS = 0x2000;
constexpr std::uint64_t CONTENTS_PLAYER = 0x40000;
constexpr std::uint64_t CONTENTS_NPC = 0x80000;
constexpr std::uint64_t CONTENTS_DEBRIS = 0x100000;
constexpr std::uint64_t MASK_SHOT = CONTENTS_SOLID | CONTENTS_HITBOXES | CONTENTS_WINDOW | CONTENTS_PASSBULLETS | CONTENTS_PLAYER | CONTENTS_NPC | CONTENTS_DEBRIS;
constexpr std::uint64_t MASK_GRENADE = (MASK_SHOT & ~CONTENTS_WINDOW) | CONTENTS_SKY;

template <typename HookContext>
class EngineTrace {
public:
    explicit EngineTrace(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    [[nodiscard]] Optional<TraceResult> traceGrenadeHull(cs2::Vector start, cs2::Vector end, void* skipEntity) const noexcept
    {
        return traceGrenadeHull(start, end, engine_trace::TraceFilterExcludedEntities{skipEntity, nullptr}, MASK_GRENADE, 4, 7);
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
            return TraceResult{fraction, endPosition, normal, handleRead && rawEntityHandle == engine_trace::kWorldEntityHandle, rawEntityHandle, handleRead};
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
