#pragma once

#include <cstddef>
#include <cstdint>
#include <GameClient/EngineTrace/EngineTraceTypes.h>
#include <MemoryPatterns/PatternTypes/EngineTracePatternTypes.h>

namespace engine_trace::native_pip {

constexpr std::uint64_t kFirstInteraction{0x0000000200003001ULL};
constexpr std::uint64_t kFilterInteractionMask{0x0000000000040200ULL};
constexpr std::uint64_t kFilterObjectMask{0x0000008000020001ULL};
constexpr std::uint8_t kCollisionGroup{0x10};
constexpr std::uint8_t kQueryByte{0x0F};

struct FilterOverlayLayout {
    std::uint8_t interactsExcludeOffset{};
    std::uint8_t interactsAsOffset{};
    std::uint8_t flagsOffset{};
    std::uint8_t candidateCollectionModeOffset{};
};

[[nodiscard]] constexpr bool isValidFilterFieldOffset(std::size_t offset, std::size_t size, std::size_t alignment = 1) noexcept
{
    return offset >= sizeof(void*) && offset % alignment == 0
        && offset <= cs2::engine_trace::kFilterCapacity && size <= cs2::engine_trace::kFilterCapacity - offset;
}

[[nodiscard]] constexpr bool hasValidFilterOverlayLayout(const FilterOverlayLayout& layout) noexcept
{
    constexpr auto kQwordSize = sizeof(std::uint64_t);
    constexpr auto kByteSize = sizeof(std::uint8_t);
    return isValidFilterFieldOffset(layout.interactsExcludeOffset, kQwordSize, alignof(std::uint64_t))
        && isValidFilterFieldOffset(layout.interactsAsOffset, kQwordSize, alignof(std::uint64_t))
        && isValidFilterFieldOffset(layout.flagsOffset, kByteSize)
        && isValidFilterFieldOffset(layout.candidateCollectionModeOffset, kByteSize)
        && !cs2::engine_trace::doRegionsOverlap(layout.interactsExcludeOffset, kQwordSize, layout.interactsAsOffset, kQwordSize)
        && !cs2::engine_trace::doRegionsOverlap(layout.interactsExcludeOffset, kQwordSize, layout.flagsOffset, kByteSize)
        && !cs2::engine_trace::doRegionsOverlap(layout.interactsExcludeOffset, kQwordSize, layout.candidateCollectionModeOffset, kByteSize)
        && !cs2::engine_trace::doRegionsOverlap(layout.interactsAsOffset, kQwordSize, layout.flagsOffset, kByteSize)
        && !cs2::engine_trace::doRegionsOverlap(layout.interactsAsOffset, kQwordSize, layout.candidateCollectionModeOffset, kByteSize)
        && !cs2::engine_trace::doRegionsOverlap(layout.flagsOffset, kByteSize, layout.candidateCollectionModeOffset, kByteSize);
}

[[nodiscard]] constexpr bool isFilterOverlayWriteOffset(const FilterOverlayLayout& layout, std::size_t offset) noexcept
{
    return hasValidFilterOverlayLayout(layout)
        && (offset >= layout.interactsExcludeOffset && offset < layout.interactsExcludeOffset + sizeof(std::uint64_t)
            || offset >= layout.interactsAsOffset && offset < layout.interactsAsOffset + sizeof(std::uint64_t)
            || offset == layout.flagsOffset || offset == layout.candidateCollectionModeOffset);
}

[[nodiscard]] inline bool applyFilterOverlay(cs2::engine_trace::TraceFilterStorage& filter, const FilterOverlayLayout& layout) noexcept
{
    if (!hasValidFilterOverlayLayout(layout))
        return false;

    cs2::engine_trace::writeFilterValue(filter, layout.interactsExcludeOffset, kFilterInteractionMask);
    cs2::engine_trace::writeFilterValue(filter, layout.interactsAsOffset, kFilterObjectMask);
    filter.storage[layout.flagsOffset] |= std::byte{0x02};
    filter.storage[layout.candidateCollectionModeOffset] = std::byte{0x01};
    return true;
}

struct TraceBindings {
    UnpackStrongTypeAliasT<TraceShapeFunctionPointer> traceShape{};
    void** managerStorage{};
    UnpackStrongTypeAliasT<InitFilterFunctionPointer> initFilter{};
    UnpackStrongTypeAliasT<AddSecondExcludedEntityToFilterFunctionPointer> addSecondExcludedEntity{};
    TraceOutputLayout outputLayout{};
    FilterOverlayLayout filterOverlayLayout{};
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
        },
        .filterOverlayLayout = {
            .interactsExcludeOffset = results.template get<CTraceFilterInteractsExcludeOffset>(),
            .interactsAsOffset = results.template get<CTraceFilterInteractsAsOffset>(),
            .flagsOffset = results.template get<CTraceFilterFlagsOffset>(),
            .candidateCollectionModeOffset = results.template get<CTraceFilterCandidateCollectionModeOffset>()
        }
    };
}

[[nodiscard]] inline bool hasValidBindings(const TraceBindings& bindings) noexcept
{
    if (bindings.traceShape == nullptr || bindings.managerStorage == nullptr || *bindings.managerStorage == nullptr
        || bindings.initFilter == nullptr || bindings.addSecondExcludedEntity == nullptr
        || !hasValidTraceOutputLayout(bindings.outputLayout) || !bindings.outputLayout.rawEntityHandleOffset.hasValue()
        || !cs2::engine_trace::isValidRawEntityHandleOffset(bindings.outputLayout.rawEntityHandleOffset.value(),
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

    if (!applyFilterOverlay(filter, bindings.filterOverlayLayout))
        return {};
    if (request.excludedEntities.second != nullptr)
        bindings.addSecondExcludedEntity(&filter, request.excludedEntities.first, request.excludedEntities.second);

    void* const managerHolder = *bindings.managerStorage;
    if (managerHolder == nullptr)
        return {};
    bindings.traceShape(managerHolder, &descriptor, &request.start, &request.end, &filter, &output);
    return decodeTraceOutput(output, bindings.outputLayout);
}

}
