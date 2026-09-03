#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <GameClient/EngineTrace/EngineTraceTypes.h>
#include <MemoryPatterns/PatternTypes/EngineTracePatternTypes.h>
#include <Utils/MemorySection.h>

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
    && PatternSearchResults::template supports<CGameTraceRawEntityHandleOffset>()
    && PatternSearchResults::template supports<CTraceFilterBaseVtablePointer>()
    && PatternSearchResults::template supports<NativePipTraceFilterBodyProfilePointer>()
    && PatternSearchResults::template supports<NativePipPushFilterConstructorPointer>()
    && PatternSearchResults::template supports<PrimaryTraceShapeCandidateProfileMarker>()
    && PatternSearchResults::template supports<SecondaryTraceShapeCandidateProfileMarker>()
    && PatternSearchResults::template supports<PrimaryToSecondaryEnumerationCallsiteMarker>()
    && PatternSearchResults::template supports<SecondaryCandidateEnumerationFunctionPointer>();

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

[[nodiscard]] inline const std::byte* baseFilterCallback(const MemorySection& clientVmtSection, void** baseVtable) noexcept
{
    if (baseVtable == nullptr || !clientVmtSection.contains(reinterpret_cast<std::uintptr_t>(baseVtable), sizeof(void*) * 2))
        return nullptr;
    if (baseVtable[1] == nullptr)
        return nullptr;
    return static_cast<const std::byte*>(baseVtable[1]);
}

[[nodiscard]] inline bool hasExpectedBaseFilterCallback(const MemorySection& clientCodeSection, const std::byte* callback) noexcept
{
    if (callback == nullptr || !clientCodeSection.contains(reinterpret_cast<std::uintptr_t>(callback), 3))
        return false;
    return callback[0] == std::byte{0xB0} && callback[1] == std::byte{0x01} && callback[2] == std::byte{0xC3};
}

[[nodiscard]] constexpr bool hasProfileMarkers(void** baseVtable, const std::byte* pipFilterBody,
    const std::byte* pushFilterConstructor, const std::byte* primaryTraceShapeCandidate,
    const std::byte* secondaryTraceShapeCandidate, const std::byte* primaryToSecondaryCallsite,
    const std::byte* secondaryCandidateEnumerationFunction) noexcept
{
    return baseVtable != nullptr && pipFilterBody != nullptr && pushFilterConstructor != nullptr
        && primaryTraceShapeCandidate != nullptr && secondaryTraceShapeCandidate != nullptr
        && primaryToSecondaryCallsite != nullptr && secondaryCandidateEnumerationFunction != nullptr;
}

[[nodiscard]] constexpr bool hasCurrentProfileStructure(std::uintptr_t primaryTraceShapeCandidate,
    std::uintptr_t secondaryTraceShapeCandidate, std::uintptr_t primaryToSecondaryCallsite,
    std::uintptr_t secondaryCandidateEnumerationFunction) noexcept
{
    return primaryToSecondaryCallsite >= primaryTraceShapeCandidate
        && secondaryTraceShapeCandidate >= secondaryCandidateEnumerationFunction
        && primaryToSecondaryCallsite - primaryTraceShapeCandidate == 0x210
        && secondaryTraceShapeCandidate - secondaryCandidateEnumerationFunction == 0x5C3;
}

[[nodiscard]] inline bool hasExpectedFilterInitialization(const cs2::engine_trace::TraceFilterStorage& filter, void** baseVtable) noexcept
{
    return cs2::engine_trace::readFilterValue<void*>(filter, 0x00) == baseVtable
        && filter.storage[0x40] == std::byte{}
        && (std::to_integer<unsigned char>(filter.storage[0x39]) & 0x80) == 0;
}

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
    void** baseVtable{};
    const std::byte* pipFilterBody{};
    const std::byte* pushFilterConstructor{};
    const std::byte* primaryTraceShapeCandidate{};
    const std::byte* secondaryTraceShapeCandidate{};
    const std::byte* primaryToSecondaryCallsite{};
    const std::byte* secondaryCandidateEnumerationFunction{};
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
        .baseVtable = results.template get<CTraceFilterBaseVtablePointer>(),
        .pipFilterBody = results.template get<NativePipTraceFilterBodyProfilePointer>(),
        .pushFilterConstructor = results.template get<NativePipPushFilterConstructorPointer>(),
        .primaryTraceShapeCandidate = results.template get<PrimaryTraceShapeCandidateProfileMarker>(),
        .secondaryTraceShapeCandidate = results.template get<SecondaryTraceShapeCandidateProfileMarker>(),
        .primaryToSecondaryCallsite = results.template get<PrimaryToSecondaryEnumerationCallsiteMarker>(),
        .secondaryCandidateEnumerationFunction = results.template get<SecondaryCandidateEnumerationFunctionPointer>()
    };
}

template <typename HookContext>
[[nodiscard]] bool hasValidBindings(HookContext& hookContext, const TraceBindings& bindings) noexcept
{
    if (bindings.traceShape == nullptr || bindings.managerStorage == nullptr || *bindings.managerStorage == nullptr
        || bindings.initFilter == nullptr || bindings.addSecondExcludedEntity == nullptr
        || !hasProfileMarkers(bindings.baseVtable, bindings.pipFilterBody, bindings.pushFilterConstructor,
            bindings.primaryTraceShapeCandidate, bindings.secondaryTraceShapeCandidate,
            bindings.primaryToSecondaryCallsite, bindings.secondaryCandidateEnumerationFunction)
        || !hasValidTraceOutputLayout(bindings.outputLayout) || !bindings.outputLayout.rawEntityHandleOffset.hasValue()
        || !cs2::engine_trace::isValidRawEntityHandleOffset(bindings.outputLayout.rawEntityHandleOffset.value(),
            bindings.outputLayout.endPositionOffset, bindings.outputLayout.normalOffset, bindings.outputLayout.fractionOffset))
        return false;

    const auto* const callback = baseFilterCallback(hookContext.clientVmtSection(), bindings.baseVtable);
    return hasExpectedBaseFilterCallback(hookContext.clientCodeSection(), callback)
        && hasCurrentProfileStructure(
            reinterpret_cast<std::uintptr_t>(bindings.primaryTraceShapeCandidate), reinterpret_cast<std::uintptr_t>(bindings.secondaryTraceShapeCandidate),
            reinterpret_cast<std::uintptr_t>(bindings.primaryToSecondaryCallsite), reinterpret_cast<std::uintptr_t>(bindings.secondaryCandidateEnumerationFunction));
}

template <typename HookContext>
[[nodiscard]] bool isTraceAvailable(HookContext& hookContext) noexcept
{
    using PatternSearchResults = std::remove_cvref_t<decltype(hookContext.patternSearchResults())>;
    if constexpr (!hasTracePatternSupport<PatternSearchResults>) {
        return false;
    } else {
        const auto bindings = resolveBindings(hookContext);
        return hasValidBindings(hookContext, bindings);
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
        if (!hasValidBindings(hookContext, bindings))
            return {};

        const auto descriptor = makeHullTraceDescriptor(request);
        cs2::engine_trace::TraceFilterStorage filter{};
        cs2::engine_trace::TraceOutputStorage output{};
        if (bindings.initFilter(&filter, request.excludedEntities.first, kFirstInteraction, kCollisionGroup, kQueryByte)
                != static_cast<void*>(&filter)
            || !hasExpectedFilterInitialization(filter, bindings.baseVtable))
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
