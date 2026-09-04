#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <GameClient/EngineTrace/EngineTraceTypes.h>
#include <MemoryPatterns/PatternTypes/EngineTracePatternTypes.h>

namespace engine_trace::native_pip {

constexpr std::uint64_t kFirstInteraction{0x0000000200003001ULL};
constexpr std::uint64_t kFilterInteractionMask{0x0000000000040200ULL};
constexpr std::uint64_t kFilterObjectMask{0x0000008000020001ULL};
constexpr std::uint8_t kCollisionGroup{0x10};
constexpr std::uint8_t kQueryByte{0x0F};

template <typename PatternSearchResults>
inline constexpr bool hasTracePatternSupport = PatternSearchResults::template supports<TraceShapeFunctionPointer>()
    && PatternSearchResults::template supports<GameTraceManagerStoragePointer>()
    && PatternSearchResults::template supports<InitFilterFunctionPointer>()
    && PatternSearchResults::template supports<AddSecondExcludedEntityToFilterFunctionPointer>()
    && PatternSearchResults::template supports<CGameTraceEndPositionOffset>()
    && PatternSearchResults::template supports<CGameTraceNormalOffset>()
    && PatternSearchResults::template supports<CGameTraceFractionOffset>()
    && PatternSearchResults::template supports<CGameTraceRawEntityHandleOffset>();

[[nodiscard]] constexpr bool isFilterWriteWhitelistedOffset(std::size_t offset) noexcept
{
    return (offset >= 0x08 && offset <= 0x1F) || (offset >= 0x34 && offset <= 0x39) || offset == 0x40;
}

static_assert(!isFilterWriteWhitelistedOffset(0x00));
static_assert(isFilterWriteWhitelistedOffset(0x08));
static_assert(isFilterWriteWhitelistedOffset(0x1F));
static_assert(!isFilterWriteWhitelistedOffset(0x20));
static_assert(!isFilterWriteWhitelistedOffset(0x33));
static_assert(isFilterWriteWhitelistedOffset(0x34));
static_assert(isFilterWriteWhitelistedOffset(0x39));
static_assert(!isFilterWriteWhitelistedOffset(0x3A));
static_assert(!isFilterWriteWhitelistedOffset(0x3F));
static_assert(isFilterWriteWhitelistedOffset(0x40));
static_assert(!isFilterWriteWhitelistedOffset(0x41));

inline void applyFilterOverlay(cs2::engine_trace::TraceFilterStorage& filter) noexcept
{
    cs2::engine_trace::writeFilterValue(filter, 0x08, kFirstInteraction);
    cs2::engine_trace::writeFilterValue(filter, 0x10, kFilterInteractionMask);
    cs2::engine_trace::writeFilterValue(filter, 0x18, kFilterObjectMask);
    cs2::engine_trace::writeFilterValue(filter, 0x34, std::uint16_t{0xFFFF});
    filter.storage[0x36] = std::byte{};
    filter.storage[0x37] = std::byte{kQueryByte};
    filter.storage[0x38] = std::byte{kCollisionGroup};
    filter.storage[0x39] = std::byte{0x4B};
    filter.storage[0x40] = std::byte{0x01};
}

struct TraceBindings {
    UnpackStrongTypeAliasT<TraceShapeFunctionPointer> traceShape{};
    void** managerStorage{};
    UnpackStrongTypeAliasT<InitFilterFunctionPointer> initFilter{};
    UnpackStrongTypeAliasT<AddSecondExcludedEntityToFilterFunctionPointer> addSecondExcludedEntity{};
    TraceOutputLayout outputLayout{};
};

template <typename HookContext>
[[nodiscard]] TraceBindings resolveBindings(HookContext& hookContext) noexcept
{
    const auto& results = hookContext.patternSearchResults();
    return {
        .traceShape = results.template get<TraceShapeFunctionPointer>(),
        .managerStorage = results.template get<GameTraceManagerStoragePointer>(),
        .initFilter = results.template get<InitFilterFunctionPointer>(),
        .addSecondExcludedEntity = results.template get<AddSecondExcludedEntityToFilterFunctionPointer>(),
        .outputLayout = {
            .endPositionOffset = results.template get<CGameTraceEndPositionOffset>(),
            .normalOffset = results.template get<CGameTraceNormalOffset>(),
            .fractionOffset = results.template get<CGameTraceFractionOffset>(),
            .rawEntityHandleOffset = results.template get<CGameTraceRawEntityHandleOffset>()
        }
    };
}

[[nodiscard]] inline bool hasValidBindings(const TraceBindings& bindings) noexcept
{
    if (bindings.traceShape == nullptr || bindings.managerStorage == nullptr || *bindings.managerStorage == nullptr
        || bindings.initFilter == nullptr || bindings.addSecondExcludedEntity == nullptr
        || !hasValidTraceOutputLayout(bindings.outputLayout) || !bindings.outputLayout.rawEntityHandleOffset.hasValue()
        || !cs2::engine_trace::isValidRawEntityHandleOffset(bindings.outputLayout.rawEntityHandleOffset.value(),
            bindings.outputLayout.endPositionOffset, bindings.outputLayout.normalOffset, bindings.outputLayout.fractionOffset))
        return false;
    return true;
}

template <typename HookContext>
[[nodiscard]] bool isTraceAvailable(HookContext& hookContext) noexcept
{
    using PatternSearchResults = std::remove_cvref_t<decltype(hookContext.patternSearchResults())>;
    if constexpr (!hasTracePatternSupport<PatternSearchResults>) {
        return false;
    } else {
        const auto bindings = resolveBindings(hookContext);
        return hasValidBindings(bindings);
    }
}

template <typename HookContext>
[[nodiscard]] Optional<TraceResult> traceHull(HookContext& hookContext, const HullTraceRequest& request) noexcept
{
    using PatternSearchResults = std::remove_cvref_t<decltype(hookContext.patternSearchResults())>;
    if constexpr (!hasTracePatternSupport<PatternSearchResults>) {
        return {};
    } else {
        if (!isValidHullTraceRequest(request))
            return {};

        const auto bindings = resolveBindings(hookContext);
        if (!hasValidBindings(bindings))
            return {};

        const auto descriptor = makeHullTraceDescriptor(request);
        cs2::engine_trace::TraceFilterStorage filter{};
        cs2::engine_trace::TraceOutputStorage output{};
        if (bindings.initFilter(&filter, request.excludedEntities.first, kFirstInteraction, kCollisionGroup, kQueryByte)
                != static_cast<void*>(&filter))
            return {};

        applyFilterOverlay(filter);
        if (request.excludedEntities.second != nullptr)
            bindings.addSecondExcludedEntity(&filter, request.excludedEntities.first, request.excludedEntities.second);

        void* const managerHolder = *bindings.managerStorage;
        if (managerHolder == nullptr)
            return {};
        bindings.traceShape(managerHolder, &descriptor, &request.start, &request.end, &filter, &output);
        return decodeTraceOutput(output, bindings.outputLayout);
    }
}

}
