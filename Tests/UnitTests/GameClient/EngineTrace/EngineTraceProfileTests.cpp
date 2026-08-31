#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include <gtest/gtest.h>

#include <GameClient/EngineTrace/EngineTrace.h>
#include <Platform/Macros/IsPlatform.h>
#include <Utils/MemorySection.h>

#if IS_WIN64()
namespace
{

enum class NativePipInitOutcome {
    Correct,
    WrongReturn,
    WrongVtable,
    Bit7Set,
    NonzeroTailFlag
};

struct NativePipTraceRecorder {
    void** baseVtable{};
    NativePipInitOutcome initOutcome{NativePipInitOutcome::Correct};
    int initCalls{};
    int secondExclusionCalls{};
    int traceShapeCalls{};
    bool initializedWithNativeArguments{};
    bool overlayAppliedBeforeSecondExclusion{};
    bool traceShapeCalledAfterSecondExclusion{};
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

template <typename T>
void writeOutput(engine_trace::OutputStorage& output, std::size_t offset, T value) noexcept
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
    activeRecorder->initializedWithNativeArguments = firstInteraction == engine_trace::kNativePipFirstInteraction
        && collisionGroup == engine_trace::kNativePipCollisionGroup && queryByte == engine_trace::kNativePipQueryByte;
    auto& filter = *static_cast<engine_trace::FilterStorage*>(storage);
    engine_trace::writeFilterValue(filter, 0x00, activeRecorder->baseVtable);
    switch (activeRecorder->initOutcome) {
    case NativePipInitOutcome::WrongReturn:
        return filter.storage + 1;
    case NativePipInitOutcome::WrongVtable:
        engine_trace::writeFilterValue(filter, 0x00, static_cast<void*>(nullptr));
        return &filter;
    case NativePipInitOutcome::Bit7Set:
        filter.storage[0x39] = std::byte{0x80};
        return &filter;
    case NativePipInitOutcome::NonzeroTailFlag:
        filter.storage[0x40] = std::byte{0x01};
        return &filter;
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
    const auto& filter = *static_cast<const engine_trace::FilterStorage*>(storage);
    activeRecorder->overlayAppliedBeforeSecondExclusion = engine_trace::readFilterValue<std::uint64_t>(filter, 0x08) == engine_trace::kNativePipFirstInteraction
        && engine_trace::readFilterValue<std::uint64_t>(filter, 0x10) == engine_trace::kNativePipFilterInteractionMask
        && engine_trace::readFilterValue<std::uint64_t>(filter, 0x18) == engine_trace::kNativePipFilterObjectMask
        && engine_trace::readFilterValue<std::uint16_t>(filter, 0x34) == 0xFFFF
        && filter.storage[0x36] == std::byte{}
        && filter.storage[0x37] == std::byte{0x0F}
        && filter.storage[0x38] == std::byte{0x10}
        && filter.storage[0x39] == std::byte{0x4B}
        && filter.storage[0x40] == std::byte{0x01};
}

[[nodiscard]] bool nativePipTraceShape(void*, const void*, const cs2::Vector*, const cs2::Vector*, void*, void* output) noexcept
{
    if (activeRecorder == nullptr)
        return false;

    ++activeRecorder->traceShapeCalls;
    activeRecorder->traceShapeCalledAfterSecondExclusion = activeRecorder->overlayAppliedBeforeSecondExclusion
        && activeRecorder->secondExclusionCalls == 1;
    auto& traceOutput = *static_cast<engine_trace::OutputStorage*>(output);
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
    BaseVtable,
    PipFilterBody,
    PushFilterConstructor,
    PrimaryTraceShapeCandidate,
    SecondaryTraceShapeCandidate,
    PrimaryToSecondaryCallsite,
    SecondaryCandidateEnumerationFunction
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
            else if constexpr (std::is_same_v<T, CTraceFilterBaseVtablePointer>)
                return context.baseVtableAvailable ? context.baseVtable : nullptr;
            else if constexpr (std::is_same_v<T, NativePipTraceFilterBodyProfilePointer>)
                return context.pipFilterBodyAvailable ? context.markerStorage.data() : nullptr;
            else if constexpr (std::is_same_v<T, NativePipPushFilterConstructorPointer>)
                return context.pushFilterConstructorAvailable ? context.markerStorage.data() : nullptr;
            else if constexpr (std::is_same_v<T, PrimaryTraceShapeCandidateProfileMarker>)
                return context.primaryTraceShapeCandidateAvailable ? context.markerStorage.data() : nullptr;
            else if constexpr (std::is_same_v<T, SecondaryTraceShapeCandidateProfileMarker>)
                return context.secondaryTraceShapeCandidateAvailable ? context.markerStorage.data() + 0x9C3 : nullptr;
            else if constexpr (std::is_same_v<T, PrimaryToSecondaryEnumerationCallsiteMarker>)
                return context.primaryToSecondaryCallsiteAvailable ? context.markerStorage.data() + context.primaryToSecondaryCallsiteOffset : nullptr;
            else {
                static_assert(std::is_same_v<T, SecondaryCandidateEnumerationFunctionPointer>);
                return context.secondaryCandidateEnumerationFunctionAvailable ? context.markerStorage.data() + context.secondaryCandidateEnumerationFunctionOffset : nullptr;
            }
        }

        NativePipEngineTraceContext& context;
    };

    NativePipEngineTraceContext() noexcept
        : clientCodeSectionStorage{std::span{callbackBytes}}
        , clientVmtSectionStorage{std::span{reinterpret_cast<const std::byte*>(baseVtable), sizeof(baseVtable)}}
        , results{*this}
    {
    }

    [[nodiscard]] const PatternSearchResults& patternSearchResults() const noexcept { return results; }
    [[nodiscard]] const MemorySection& clientCodeSection() const noexcept { return clientCodeSectionStorage; }
    [[nodiscard]] const MemorySection& clientVmtSection() const noexcept { return clientVmtSectionStorage; }

    void setDependencyAvailable(NativePipDependency dependency, bool available) noexcept
    {
        switch (dependency) {
        case NativePipDependency::TraceShape: traceShapeAvailable = available; break;
        case NativePipDependency::ManagerStorage: managerStorageAvailable = available; break;
        case NativePipDependency::InitFilter: initFilterAvailable = available; break;
        case NativePipDependency::SecondExclusion: secondExclusionAvailable = available; break;
        case NativePipDependency::BaseVtable: baseVtableAvailable = available; break;
        case NativePipDependency::PipFilterBody: pipFilterBodyAvailable = available; break;
        case NativePipDependency::PushFilterConstructor: pushFilterConstructorAvailable = available; break;
        case NativePipDependency::PrimaryTraceShapeCandidate: primaryTraceShapeCandidateAvailable = available; break;
        case NativePipDependency::SecondaryTraceShapeCandidate: secondaryTraceShapeCandidateAvailable = available; break;
        case NativePipDependency::PrimaryToSecondaryCallsite: primaryToSecondaryCallsiteAvailable = available; break;
        case NativePipDependency::SecondaryCandidateEnumerationFunction: secondaryCandidateEnumerationFunctionAvailable = available; break;
        }
    }

    std::array<std::byte, 3> callbackBytes{std::byte{0xB0}, std::byte{0x01}, std::byte{0xC3}};
    void* baseVtable[2]{nullptr, callbackBytes.data()};
    std::byte managerObject{};
    void* managerHolder{&managerObject};
    std::array<std::byte, 0xA00> markerStorage{};
    MemorySection clientCodeSectionStorage;
    MemorySection clientVmtSectionStorage;
    PatternSearchResults results;
    bool traceShapeAvailable{true};
    bool managerStorageAvailable{true};
    bool initFilterAvailable{true};
    bool secondExclusionAvailable{true};
    bool baseVtableAvailable{true};
    bool pipFilterBodyAvailable{true};
    bool pushFilterConstructorAvailable{true};
    bool primaryTraceShapeCandidateAvailable{true};
    bool secondaryTraceShapeCandidateAvailable{true};
    bool primaryToSecondaryCallsiteAvailable{true};
    bool secondaryCandidateEnumerationFunctionAvailable{true};
    std::int32_t endPositionOffset{0x10};
    std::int32_t normalOffset{0x20};
    std::int32_t fractionOffset{0x30};
    std::uint8_t rawEntityHandleOffset{0x40};
    std::size_t primaryToSecondaryCallsiteOffset{0x210};
    std::size_t secondaryCandidateEnumerationFunctionOffset{0x400};
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

TEST(EngineTraceNativePipProfileTest, RequiresEveryResolvedProfileDependency)
{
    constexpr std::array dependencies{
        NativePipDependency::TraceShape,
        NativePipDependency::ManagerStorage,
        NativePipDependency::InitFilter,
        NativePipDependency::SecondExclusion,
        NativePipDependency::BaseVtable,
        NativePipDependency::PipFilterBody,
        NativePipDependency::PushFilterConstructor,
        NativePipDependency::PrimaryTraceShapeCandidate,
        NativePipDependency::SecondaryTraceShapeCandidate,
        NativePipDependency::PrimaryToSecondaryCallsite,
        NativePipDependency::SecondaryCandidateEnumerationFunction
    };

    for (const auto dependency : dependencies) {
        NativePipEngineTraceContext context;
        NativePipTraceRecorder recorder{context.baseVtable};
        ActiveRecorderGuard activeRecorderGuard{recorder};
        context.setDependencyAvailable(dependency, false);
        NativePipEngineTrace trace{context};

        EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
        EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
        EXPECT_EQ(recorder.traceShapeCalls, 0);
    }
}

TEST(EngineTraceNativePipProfileTest, ReducedPatternResultsAreUnavailableWithoutNativeCalls)
{
    ReducedNativePipEngineTraceContext context;
    EngineTrace trace{context};

    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
}

TEST(EngineTraceNativePipProfileTest, RejectsNonFiniteInFlightInputsBeforeNativeCalls)
{
    constexpr auto nan = std::bit_cast<float>(std::uint32_t{0x7FC00000});
    constexpr auto infinity = std::bit_cast<float>(std::uint32_t{0x7F800000});
    constexpr std::array invalidVectors{
        cs2::Vector{nan, 0.0f, 0.0f},
        cs2::Vector{infinity, 0.0f, 0.0f}
    };

    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    ActiveRecorderGuard activeRecorderGuard{recorder};
    NativePipEngineTrace trace{context};

    for (const auto invalid : invalidVectors) {
        EXPECT_FALSE(trace.traceInFlightGrenadeHull(invalid, {}, {}).hasValue());
        EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, invalid, {}).hasValue());
    }

    EXPECT_EQ(recorder.initCalls, 0);
    EXPECT_EQ(recorder.secondExclusionCalls, 0);
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceNativePipProfileTest, RequiresValidOutputAndManagerDependencies)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    ActiveRecorderGuard activeRecorderGuard{recorder};
    NativePipEngineTrace trace{context};

    context.endPositionOffset = 0;
    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    context.endPositionOffset = 0x10;
    context.normalOffset = 0;
    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    context.normalOffset = 0x20;
    context.fractionOffset = 0;
    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    context.fractionOffset = 0x30;
    context.rawEntityHandleOffset = 0;
    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    context.rawEntityHandleOffset = 0x40;
    context.managerHolder = nullptr;
    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceNativePipProfileTest, RejectsOutOfRangeCallbackBeforeReadingItsBytes)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    ActiveRecorderGuard activeRecorderGuard{recorder};
    context.clientCodeSectionStorage = MemorySection{};
    NativePipEngineTrace trace{context};

    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceNativePipProfileTest, RejectsBaseVtableOutsideTheClientVmtSection)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    ActiveRecorderGuard activeRecorderGuard{recorder};
    context.clientVmtSectionStorage = {};
    NativePipEngineTrace trace{context};

    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceNativePipProfileTest, RejectsBaseVtableWithoutTwoReadableEntries)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    ActiveRecorderGuard activeRecorderGuard{recorder};
    context.clientVmtSectionStorage = MemorySection{std::span{reinterpret_cast<const std::byte*>(context.baseVtable), sizeof(void*)}};
    NativePipEngineTrace trace{context};

    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceNativePipProfileTest, RejectsUnexpectedCallbackBytesWithoutTracing)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    ActiveRecorderGuard activeRecorderGuard{recorder};
    context.callbackBytes[1] = std::byte{};
    NativePipEngineTrace trace{context};

    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceNativePipProfileTest, RejectsUnexpectedPrimaryStructuralDeltaWithoutTracing)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    ActiveRecorderGuard activeRecorderGuard{recorder};
    context.primaryToSecondaryCallsiteOffset = 0x20F;
    NativePipEngineTrace trace{context};

    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceNativePipProfileTest, RejectsWrongProfileCalleeWithoutTracing)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    ActiveRecorderGuard activeRecorderGuard{recorder};
    context.secondaryCandidateEnumerationFunctionOffset = 0x401;
    NativePipEngineTrace trace{context};

    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceNativePipProfileTest, RejectsInvalidFilterInitializationBeforeTracing)
{
    constexpr std::array outcomes{
        NativePipInitOutcome::WrongReturn,
        NativePipInitOutcome::WrongVtable,
        NativePipInitOutcome::Bit7Set,
        NativePipInitOutcome::NonzeroTailFlag
    };

    for (const auto outcome : outcomes) {
        NativePipEngineTraceContext context;
        NativePipTraceRecorder recorder{context.baseVtable, outcome};
        ActiveRecorderGuard activeRecorderGuard{recorder};
        NativePipEngineTrace trace{context};

        EXPECT_TRUE(trace.isInFlightGrenadeTraceAvailable());
        EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
        EXPECT_EQ(recorder.initCalls, 1);
        EXPECT_EQ(recorder.secondExclusionCalls, 0);
        EXPECT_EQ(recorder.traceShapeCalls, 0);
    }
}

TEST(EngineTraceNativePipProfileTest, AppliesOverlayThenSecondExclusionBeforeTracing)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    ActiveRecorderGuard activeRecorderGuard{recorder};
    NativePipEngineTrace trace{context};
    std::byte firstExcluded{};
    std::byte secondExcluded{};

    const auto result = trace.traceInFlightGrenadeHull({}, {}, {&firstExcluded, &secondExcluded});

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(recorder.initCalls, 1);
    EXPECT_TRUE(recorder.initializedWithNativeArguments);
    EXPECT_EQ(recorder.secondExclusionCalls, 1);
    EXPECT_EQ(recorder.traceShapeCalls, 1);
    EXPECT_TRUE(recorder.overlayAppliedBeforeSecondExclusion);
    EXPECT_TRUE(recorder.traceShapeCalledAfterSecondExclusion);
}

}
#endif
