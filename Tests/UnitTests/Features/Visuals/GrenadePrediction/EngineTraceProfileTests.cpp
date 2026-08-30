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
        , results{*this}
    {
    }

    [[nodiscard]] const PatternSearchResults& patternSearchResults() const noexcept { return results; }
    [[nodiscard]] const MemorySection& clientCodeSection() const noexcept { return clientCodeSectionStorage; }

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
        activeRecorder = &recorder;
        context.setDependencyAvailable(dependency, false);
        NativePipEngineTrace trace{context};

        EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
        EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
        EXPECT_EQ(recorder.traceShapeCalls, 0);
    }
    activeRecorder = nullptr;
}

TEST(EngineTraceNativePipProfileTest, RequiresValidOutputAndManagerDependencies)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    activeRecorder = &recorder;
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
    activeRecorder = nullptr;
}

TEST(EngineTraceNativePipProfileTest, RejectsOutOfRangeCallbackBeforeReadingItsBytes)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    activeRecorder = &recorder;
    context.clientCodeSectionStorage = MemorySection{};
    NativePipEngineTrace trace{context};

    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
    activeRecorder = nullptr;
}

TEST(EngineTraceNativePipProfileTest, RejectsUnexpectedCallbackBytesWithoutTracing)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    activeRecorder = &recorder;
    context.callbackBytes[1] = std::byte{};
    NativePipEngineTrace trace{context};

    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
    activeRecorder = nullptr;
}

TEST(EngineTraceNativePipProfileTest, RejectsUnexpectedPrimaryStructuralDeltaWithoutTracing)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    activeRecorder = &recorder;
    context.primaryToSecondaryCallsiteOffset = 0x20F;
    NativePipEngineTrace trace{context};

    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
    activeRecorder = nullptr;
}

TEST(EngineTraceNativePipProfileTest, RejectsWrongProfileCalleeWithoutTracing)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    activeRecorder = &recorder;
    context.secondaryCandidateEnumerationFunctionOffset = 0x401;
    NativePipEngineTrace trace{context};

    EXPECT_FALSE(trace.isInFlightGrenadeTraceAvailable());
    EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
    activeRecorder = nullptr;
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
        activeRecorder = &recorder;
        NativePipEngineTrace trace{context};

        EXPECT_TRUE(trace.isInFlightGrenadeTraceAvailable());
        EXPECT_FALSE(trace.traceInFlightGrenadeHull({}, {}, {}).hasValue());
        EXPECT_EQ(recorder.initCalls, 1);
        EXPECT_EQ(recorder.secondExclusionCalls, 0);
        EXPECT_EQ(recorder.traceShapeCalls, 0);
    }
    activeRecorder = nullptr;
}

TEST(EngineTraceNativePipProfileTest, AppliesOverlayThenSecondExclusionBeforeTracing)
{
    NativePipEngineTraceContext context;
    NativePipTraceRecorder recorder{context.baseVtable};
    activeRecorder = &recorder;
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
    activeRecorder = nullptr;
}

}
#endif
