#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include <GameClient/EngineTrace/EngineTrace.h>
#include <GameClient/EngineTrace/NativePipTrace.h>

namespace
{

constexpr engine_trace::native_pip::FilterOverlayLayout kCanonicalFilterOverlayLayout{
    .interactsExcludeOffset = 0x10,
    .interactsAsOffset = 0x18,
    .flagsOffset = 0x39,
    .candidateCollectionModeOffset = 0x40
};

enum class NativePipInitOutcome {
    Correct,
    WrongReturn
};

struct NativePipTraceRecorder {
    NativePipInitOutcome initOutcome{NativePipInitOutcome::Correct};
    void** managerStorage{};
    bool clearManagerOnInit{};
    int initCalls{};
    int secondExclusionCalls{};
    int traceShapeCalls{};
    bool initializedWithNativeArguments{};
    bool constructorFieldsPreservedBeforeSecondExclusion{};
    bool overlayAppliedBeforeSecondExclusion{};
    bool traceShapeCalledAfterSecondExclusion{};
    engine_trace::native_pip::FilterOverlayLayout filterOverlayLayout{kCanonicalFilterOverlayLayout};
};

NativePipTraceRecorder* activeRecorder{};

class ActiveRecorderGuard {
public:
    explicit ActiveRecorderGuard(NativePipTraceRecorder& recorder) noexcept
        : recorder{recorder}
    {
        activeRecorder = &recorder;
    }

    ~ActiveRecorderGuard()
    {
        if (activeRecorder == &recorder)
            activeRecorder = nullptr;
    }

    ActiveRecorderGuard(const ActiveRecorderGuard&) = delete;
    ActiveRecorderGuard& operator=(const ActiveRecorderGuard&) = delete;

private:
    NativePipTraceRecorder& recorder;
};

[[nodiscard]] constexpr engine_trace::HullTraceRequest nativePipRequest(cs2::Vector start, cs2::Vector end,
    engine_trace::TraceFilterExcludedEntities excludedEntities = {}) noexcept
{
    return {
        .start = start,
        .end = end,
        .mins = {-2.0f, -2.0f, -2.0f},
        .maxs = {2.0f, 2.0f, 2.0f},
        .excludedEntities = excludedEntities
    };
}

template <typename T>
void writeOutput(cs2::engine_trace::TraceOutputStorage& output, std::size_t offset, T value) noexcept
{
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    for (std::size_t i{}; i < sizeof(T); ++i)
        output.storage[offset + i] = bytes[i];
}

[[nodiscard]] void* nativePipInitFilter(void* storage, void*, std::uint64_t firstInteraction, std::uint8_t collisionGroup, std::uint8_t queryByte) noexcept
{
    if (activeRecorder == nullptr)
        return nullptr;

    ++activeRecorder->initCalls;
    if (activeRecorder->clearManagerOnInit)
        *activeRecorder->managerStorage = nullptr;
    activeRecorder->initializedWithNativeArguments = firstInteraction == engine_trace::native_pip::kFirstInteraction
        && collisionGroup == engine_trace::native_pip::kCollisionGroup && queryByte == engine_trace::native_pip::kQueryByte;
    auto& filter = *static_cast<cs2::engine_trace::TraceFilterStorage*>(storage);
    cs2::engine_trace::writeFilterValue(filter, 0x08, firstInteraction);
    cs2::engine_trace::writeFilterValue(filter, 0x34, std::uint16_t{0xFFFF});
    filter.storage[0x36] = std::byte{};
    filter.storage[0x37] = std::byte{queryByte};
    filter.storage[0x38] = std::byte{collisionGroup};
    filter.storage[activeRecorder->filterOverlayLayout.flagsOffset] = std::byte{0x49};
    filter.storage[activeRecorder->filterOverlayLayout.candidateCollectionModeOffset] = std::byte{0xA5};
    switch (activeRecorder->initOutcome) {
    case NativePipInitOutcome::WrongReturn:
        return filter.storage + 1;
    case NativePipInitOutcome::Correct:
        return &filter;
    }
    return nullptr;
}

void nativePipAddSecondExcludedEntity(void* storage, void*, void*) noexcept
{
    if (activeRecorder == nullptr)
        return;

    ++activeRecorder->secondExclusionCalls;
    const auto& filter = *static_cast<const cs2::engine_trace::TraceFilterStorage*>(storage);
    activeRecorder->constructorFieldsPreservedBeforeSecondExclusion = cs2::engine_trace::readFilterValue<std::uint64_t>(filter, 0x08) == engine_trace::native_pip::kFirstInteraction
        && cs2::engine_trace::readFilterValue<std::uint16_t>(filter, 0x34) == 0xFFFF
        && filter.storage[0x36] == std::byte{}
        && filter.storage[0x37] == std::byte{engine_trace::native_pip::kQueryByte}
        && filter.storage[0x38] == std::byte{engine_trace::native_pip::kCollisionGroup};
    const auto& layout = activeRecorder->filterOverlayLayout;
    activeRecorder->overlayAppliedBeforeSecondExclusion = cs2::engine_trace::readFilterValue<std::uint64_t>(filter, layout.interactsExcludeOffset) == engine_trace::native_pip::kFilterInteractionMask
        && cs2::engine_trace::readFilterValue<std::uint64_t>(filter, layout.interactsAsOffset) == engine_trace::native_pip::kFilterObjectMask
        && filter.storage[layout.flagsOffset] == std::byte{0x4B}
        && filter.storage[layout.candidateCollectionModeOffset] == std::byte{0x01};
}

[[nodiscard]] bool nativePipTraceShape(void*, const void*, const cs2::Vector*, const cs2::Vector*, void*, void* output) noexcept
{
    if (activeRecorder == nullptr)
        return false;

    ++activeRecorder->traceShapeCalls;
    activeRecorder->traceShapeCalledAfterSecondExclusion = activeRecorder->constructorFieldsPreservedBeforeSecondExclusion
        && activeRecorder->overlayAppliedBeforeSecondExclusion
        && activeRecorder->secondExclusionCalls == 1;
    auto& traceOutput = *static_cast<cs2::engine_trace::TraceOutputStorage*>(output);
    writeOutput(traceOutput, 0x10, cs2::Vector{1.0f, 2.0f, 3.0f});
    writeOutput(traceOutput, 0x20, cs2::Vector{});
    writeOutput(traceOutput, 0x30, 1.0f);
    return true;
}

enum class NativePipDependency {
    TraceShape,
    ManagerStorage,
    InitFilter,
    SecondExclusion,
    EndPosition,
    Normal,
    Fraction,
    RawEntityHandle,
    InteractsExclude,
    InteractsAs,
    Flags,
    CandidateCollectionMode
};

struct NativePipEngineTraceContext {
    struct PatternSearchResults {
        template <typename>
        [[nodiscard]] static consteval bool supports() noexcept
        {
            return true;
        }

        template <typename T>
        [[nodiscard]] auto get() const noexcept
        {
            ++context.patternGetCalls;
            if constexpr (std::is_same_v<T, TraceShapeFunctionPointer>)
                return context.traceShapeAvailable ? &nativePipTraceShape : nullptr;
            else if constexpr (std::is_same_v<T, GameTraceManagerStoragePointer>)
                return context.managerStorageAvailable ? &context.managerHolder : nullptr;
            else if constexpr (std::is_same_v<T, InitFilterFunctionPointer>)
                return context.initFilterAvailable ? &nativePipInitFilter : nullptr;
            else if constexpr (std::is_same_v<T, AddSecondExcludedEntityToFilterFunctionPointer>)
                return context.secondExclusionAvailable ? &nativePipAddSecondExcludedEntity : nullptr;
            else if constexpr (std::is_same_v<T, CGameTraceEndPositionOffset>)
                return context.endPositionOffset;
            else if constexpr (std::is_same_v<T, CGameTraceNormalOffset>)
                return context.normalOffset;
            else if constexpr (std::is_same_v<T, CGameTraceFractionOffset>)
                return context.fractionOffset;
            else if constexpr (std::is_same_v<T, CGameTraceRawEntityHandleOffset>)
                return context.rawEntityHandleOffset;
            else if constexpr (std::is_same_v<T, CTraceFilterInteractsExcludeOffset>)
                return context.filterOverlayLayout.interactsExcludeOffset;
            else if constexpr (std::is_same_v<T, CTraceFilterInteractsAsOffset>)
                return context.filterOverlayLayout.interactsAsOffset;
            else if constexpr (std::is_same_v<T, CTraceFilterFlagsOffset>)
                return context.filterOverlayLayout.flagsOffset;
            else {
                static_assert(std::is_same_v<T, CTraceFilterCandidateCollectionModeOffset>);
                return context.filterOverlayLayout.candidateCollectionModeOffset;
            }
        }

        NativePipEngineTraceContext& context;
    };

    NativePipEngineTraceContext() noexcept
        : results{*this}
    {
    }

    [[nodiscard]] const PatternSearchResults& patternSearchResults() const noexcept
    {
        ++patternSearchResultsCalls;
        return results;
    }
    void setDependencyAvailable(NativePipDependency dependency, bool available) noexcept
    {
        switch (dependency) {
        case NativePipDependency::TraceShape: traceShapeAvailable = available; break;
        case NativePipDependency::ManagerStorage: managerStorageAvailable = available; break;
        case NativePipDependency::InitFilter: initFilterAvailable = available; break;
        case NativePipDependency::SecondExclusion: secondExclusionAvailable = available; break;
        case NativePipDependency::EndPosition: endPositionOffset = available ? 0x10 : 0; break;
        case NativePipDependency::Normal: normalOffset = available ? 0x20 : 0; break;
        case NativePipDependency::Fraction: fractionOffset = available ? 0x30 : 0; break;
        case NativePipDependency::RawEntityHandle: rawEntityHandleOffset = available ? 0x40 : 0; break;
        case NativePipDependency::InteractsExclude: filterOverlayLayout.interactsExcludeOffset = available ? kCanonicalFilterOverlayLayout.interactsExcludeOffset : 0; break;
        case NativePipDependency::InteractsAs: filterOverlayLayout.interactsAsOffset = available ? kCanonicalFilterOverlayLayout.interactsAsOffset : 0; break;
        case NativePipDependency::Flags: filterOverlayLayout.flagsOffset = available ? kCanonicalFilterOverlayLayout.flagsOffset : 0; break;
        case NativePipDependency::CandidateCollectionMode: filterOverlayLayout.candidateCollectionModeOffset = available ? kCanonicalFilterOverlayLayout.candidateCollectionModeOffset : 0; break;
        }
    }

    std::byte managerObject{};
    void* managerHolder{&managerObject};
    mutable int patternSearchResultsCalls{};
    mutable int patternGetCalls{};
    PatternSearchResults results;
    bool traceShapeAvailable{true};
    bool managerStorageAvailable{true};
    bool initFilterAvailable{true};
    bool secondExclusionAvailable{true};
    std::int32_t endPositionOffset{0x10};
    std::int32_t normalOffset{0x20};
    std::int32_t fractionOffset{0x30};
    std::uint8_t rawEntityHandleOffset{0x40};
    engine_trace::native_pip::FilterOverlayLayout filterOverlayLayout{kCanonicalFilterOverlayLayout};
};

using NativePipEngineTrace = EngineTrace<NativePipEngineTraceContext>;

struct ReducedNativePipEngineTraceContext {
    struct PatternSearchResults {
        template <typename>
        [[nodiscard]] static consteval bool supports() noexcept
        {
            return false;
        }
    };

    [[nodiscard]] const PatternSearchResults& patternSearchResults() const noexcept { return results; }

    PatternSearchResults results;
};

TEST(EngineTraceNativePipTest, RequiresEveryOperationalBinding)
{
    constexpr std::array dependencies{
        NativePipDependency::TraceShape,
        NativePipDependency::ManagerStorage,
        NativePipDependency::InitFilter,
        NativePipDependency::SecondExclusion,
        NativePipDependency::EndPosition,
        NativePipDependency::Normal,
        NativePipDependency::Fraction,
        NativePipDependency::RawEntityHandle,
        NativePipDependency::InteractsExclude,
        NativePipDependency::InteractsAs,
        NativePipDependency::Flags,
        NativePipDependency::CandidateCollectionMode
    };

    for (const auto dependency : dependencies) {
        NativePipEngineTraceContext context;
        NativePipTraceRecorder recorder;
        ActiveRecorderGuard activeRecorderGuard{recorder};
        context.setDependencyAvailable(dependency, false);
        NativePipEngineTrace trace{context};

        EXPECT_FALSE(trace.isNativePipHullTraceAvailable());
        EXPECT_FALSE(trace.traceNativePipHull(nativePipRequest({}, {})).hasValue());
        EXPECT_EQ(recorder.initCalls, 0);
        EXPECT_EQ(recorder.secondExclusionCalls, 0);
        EXPECT_EQ(recorder.traceShapeCalls, 0);
    }
}

TEST(EngineTraceNativePipTest, ReducedPatternResultsAreUnavailableWithoutNativeCalls)
{
    ReducedNativePipEngineTraceContext context;
    EngineTrace trace{context};

    EXPECT_FALSE(trace.isNativePipHullTraceAvailable());
    EXPECT_FALSE(trace.traceNativePipHull(nativePipRequest({}, {})).hasValue());
}

TEST(EngineTraceNativePipTest, RejectsNonFiniteInFlightInputsBeforeNativeCalls)
{
    constexpr auto nan = std::bit_cast<float>(std::uint32_t{0x7FC00000});
    constexpr auto infinity = std::bit_cast<float>(std::uint32_t{0x7F800000});
    constexpr std::array invalidVectors{
        cs2::Vector{nan, 0.0f, 0.0f},
        cs2::Vector{infinity, 0.0f, 0.0f}
    };

    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    NativePipEngineTrace trace{context};

    for (const auto invalid : invalidVectors) {
        EXPECT_FALSE(trace.traceNativePipHull(nativePipRequest(invalid, {})).hasValue());
        EXPECT_FALSE(trace.traceNativePipHull(nativePipRequest({}, invalid)).hasValue());
    }

    EXPECT_EQ(recorder.initCalls, 0);
    EXPECT_EQ(recorder.secondExclusionCalls, 0);
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceNativePipTest, RequiresValidOutputAndManagerDependencies)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    NativePipEngineTrace trace{context};

    context.endPositionOffset = 0;
    EXPECT_FALSE(trace.isNativePipHullTraceAvailable());
    context.endPositionOffset = 0x10;
    context.normalOffset = 0;
    EXPECT_FALSE(trace.isNativePipHullTraceAvailable());
    context.normalOffset = 0x20;
    context.fractionOffset = 0;
    EXPECT_FALSE(trace.isNativePipHullTraceAvailable());
    context.fractionOffset = 0x30;
    context.rawEntityHandleOffset = 0;
    EXPECT_FALSE(trace.isNativePipHullTraceAvailable());
    context.rawEntityHandleOffset = 0x40;
    context.managerHolder = nullptr;
    EXPECT_FALSE(trace.isNativePipHullTraceAvailable());
    EXPECT_FALSE(trace.traceNativePipHull(nativePipRequest({}, {})).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceNativePipTest, InvalidFilterLayoutsAreUnavailableBeforeNativeCalls)
{
    constexpr std::array invalidLayouts{
        engine_trace::native_pip::FilterOverlayLayout{
            .interactsExcludeOffset = 0,
            .interactsAsOffset = 0x18,
            .flagsOffset = 0x39,
            .candidateCollectionModeOffset = 0x40
        },
        engine_trace::native_pip::FilterOverlayLayout{
            .interactsExcludeOffset = 0x48,
            .interactsAsOffset = 0x18,
            .flagsOffset = 0x39,
            .candidateCollectionModeOffset = 0x40
        },
        engine_trace::native_pip::FilterOverlayLayout{
            .interactsExcludeOffset = 0x11,
            .interactsAsOffset = 0x18,
            .flagsOffset = 0x39,
            .candidateCollectionModeOffset = 0x40
        },
        engine_trace::native_pip::FilterOverlayLayout{
            .interactsExcludeOffset = 0x10,
            .interactsAsOffset = 0x18,
            .flagsOffset = 0x18,
            .candidateCollectionModeOffset = 0x40
        }
    };

    for (const auto layout : invalidLayouts) {
        NativePipEngineTraceContext context;
        NativePipTraceRecorder recorder;
        ActiveRecorderGuard activeRecorderGuard{recorder};
        context.filterOverlayLayout = layout;
        NativePipEngineTrace trace{context};

        EXPECT_FALSE(trace.isNativePipHullTraceAvailable());
        EXPECT_FALSE(trace.traceNativePipHull(nativePipRequest({}, {})).hasValue());
        EXPECT_EQ(recorder.initCalls, 0);
        EXPECT_EQ(recorder.secondExclusionCalls, 0);
        EXPECT_EQ(recorder.traceShapeCalls, 0);
    }
}

TEST(EngineTraceNativePipTest, RejectsWrongFilterInitializationReturnBeforeTracing)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{.initOutcome = NativePipInitOutcome::WrongReturn};
    ActiveRecorderGuard activeRecorderGuard{recorder};
    NativePipEngineTrace trace{context};

    EXPECT_TRUE(trace.isNativePipHullTraceAvailable());
    EXPECT_FALSE(trace.traceNativePipHull(nativePipRequest({}, {})).hasValue());
    EXPECT_EQ(recorder.initCalls, 1);
    EXPECT_EQ(recorder.secondExclusionCalls, 0);
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceNativePipTest, AppliesOverlayThenSecondExclusionBeforeTracing)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    NativePipEngineTrace trace{context};
    std::byte firstExcluded{};
    std::byte secondExcluded{};

    const auto result = trace.traceNativePipHull(nativePipRequest({}, {}, {&firstExcluded, &secondExcluded}));

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(recorder.initCalls, 1);
    EXPECT_TRUE(recorder.initializedWithNativeArguments);
    EXPECT_EQ(recorder.secondExclusionCalls, 1);
    EXPECT_EQ(recorder.traceShapeCalls, 1);
    EXPECT_TRUE(recorder.constructorFieldsPreservedBeforeSecondExclusion);
    EXPECT_TRUE(recorder.overlayAppliedBeforeSecondExclusion);
    EXPECT_TRUE(recorder.traceShapeCalledAfterSecondExclusion);
}

TEST(EngineTraceNativePipTest, UsesOneBindingSnapshotForTheWholeTraceCall)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    NativePipEngineTrace trace{context};

    const auto result = trace.traceNativePipHull(nativePipRequest({}, {}));

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(context.patternSearchResultsCalls, 1);
    EXPECT_EQ(context.patternGetCalls, 12);
    EXPECT_EQ(recorder.initCalls, 1);
    EXPECT_EQ(recorder.secondExclusionCalls, 0);
    EXPECT_EQ(recorder.traceShapeCalls, 1);
}

TEST(EngineTraceNativePipTest, FailsClosedWhenManagerBecomesNullBeforeTraceInvocation)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{
        .managerStorage = &context.managerHolder,
        .clearManagerOnInit = true
    };
    ActiveRecorderGuard activeRecorderGuard{recorder};
    NativePipEngineTrace trace{context};

    EXPECT_FALSE(trace.traceNativePipHull(nativePipRequest({}, {})).hasValue());
    EXPECT_EQ(recorder.initCalls, 1);
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

}
