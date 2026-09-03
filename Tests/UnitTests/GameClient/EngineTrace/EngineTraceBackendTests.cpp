#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include <GameClient/EngineTrace/EngineTrace.h>

namespace
{

enum class FilterInitResult {
    Storage,
    Null,
    Foreign
};

struct RegularTraceRecorder {
    FilterInitResult initResult{FilterInitResult::Storage};
    bool clearManagerOnInit{};
    bool changeOutputOffsetsOnInit{};
    void** managerStorage{};
    std::int32_t* endPositionOffset{};
    int initCalls{};
    int addSecondExclusionCalls{};
    int traceCalls{};
};

RegularTraceRecorder* activeRecorder{};

[[nodiscard]] void* regularInitFilter(void* storage, void*, std::uint64_t, std::uint8_t, std::uint8_t) noexcept
{
    if (activeRecorder == nullptr)
        return nullptr;

    ++activeRecorder->initCalls;
    if (activeRecorder->clearManagerOnInit)
        *activeRecorder->managerStorage = nullptr;
    if (activeRecorder->changeOutputOffsetsOnInit)
        *activeRecorder->endPositionOffset = 0;
    if (activeRecorder->initResult == FilterInitResult::Null)
        return nullptr;
    if (activeRecorder->initResult == FilterInitResult::Foreign)
        return static_cast<cs2::engine_trace::TraceFilterStorage*>(storage)->storage + 1;
    return storage;
}

void regularAddSecondExcludedEntity(void*, void*, void*) noexcept
{
    if (activeRecorder != nullptr)
        ++activeRecorder->addSecondExclusionCalls;
}

template <typename T>
void writeOutput(cs2::engine_trace::TraceOutputStorage& output, std::size_t offset, T value) noexcept
{
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    for (std::size_t i{}; i < sizeof(T); ++i)
        output.storage[offset + i] = bytes[i];
}

[[nodiscard]] bool regularTraceShape(void*, const void*, const cs2::Vector*, const cs2::Vector*, void*, void* output) noexcept
{
    if (activeRecorder == nullptr)
        return false;

    ++activeRecorder->traceCalls;
    auto& traceOutput = *static_cast<cs2::engine_trace::TraceOutputStorage*>(output);
    writeOutput(traceOutput, 0x10, cs2::Vector{1.0f, 2.0f, 3.0f});
    writeOutput(traceOutput, 0x20, cs2::Vector{});
    writeOutput(traceOutput, 0x30, 1.0f);
    return true;
}

struct RegularEngineTraceContext {
    struct PatternSearchResults {
        template <typename T>
        [[nodiscard]] static consteval bool supports() noexcept
        {
            return std::is_same_v<T, TraceShapeFunctionPointer>
                || std::is_same_v<T, GameTraceManagerStoragePointer>
                || std::is_same_v<T, InitFilterFunctionPointer>
                || std::is_same_v<T, AddSecondExcludedEntityToFilterFunctionPointer>
                || std::is_same_v<T, CGameTraceEndPositionOffset>
                || std::is_same_v<T, CGameTraceNormalOffset>
                || std::is_same_v<T, CGameTraceFractionOffset>
                || std::is_same_v<T, CGameTraceRawEntityHandleOffset>;
        }

        template <typename T>
        [[nodiscard]] auto get() const noexcept
        {
            ++context.patternGetCalls;
            if constexpr (std::is_same_v<T, TraceShapeFunctionPointer>)
                return &regularTraceShape;
            else if constexpr (std::is_same_v<T, GameTraceManagerStoragePointer>)
                return &context.managerHolder;
            else if constexpr (std::is_same_v<T, InitFilterFunctionPointer>)
                return &regularInitFilter;
            else if constexpr (std::is_same_v<T, AddSecondExcludedEntityToFilterFunctionPointer>)
                return &regularAddSecondExcludedEntity;
            else if constexpr (std::is_same_v<T, CGameTraceEndPositionOffset>)
                return context.endPositionOffset;
            else if constexpr (std::is_same_v<T, CGameTraceNormalOffset>)
                return context.normalOffset;
            else if constexpr (std::is_same_v<T, CGameTraceFractionOffset>)
                return context.fractionOffset;
            else {
                static_assert(std::is_same_v<T, CGameTraceRawEntityHandleOffset>);
                return context.rawEntityHandleOffset;
            }
        }

        RegularEngineTraceContext& context;
    };

    RegularEngineTraceContext() noexcept
        : results{*this}
    {
    }

    [[nodiscard]] const PatternSearchResults& patternSearchResults() const noexcept
    {
        ++patternSearchResultsCalls;
        return results;
    }

    std::byte managerObject{};
    void* managerHolder{&managerObject};
    mutable int patternSearchResultsCalls{};
    mutable int patternGetCalls{};
    std::int32_t endPositionOffset{0x10};
    std::int32_t normalOffset{0x20};
    std::int32_t fractionOffset{0x30};
    std::uint8_t rawEntityHandleOffset{0x40};
    PatternSearchResults results;
};

[[nodiscard]] constexpr engine_trace::HullTraceRequest makeRequest(engine_trace::TraceFilterExcludedEntities excludedEntities = {}) noexcept
{
    return {
        .mins = {-2.0f, -2.0f, -2.0f},
        .maxs = {2.0f, 2.0f, 2.0f},
        .excludedEntities = excludedEntities,
        .filter = {.mask = engine_trace::kMaskGrenade, .collisionGroup = 4, .queryByte = 7}
    };
}

class ActiveRecorderGuard {
public:
    explicit ActiveRecorderGuard(RegularTraceRecorder& recorder) noexcept
        : recorder{recorder}
    {
        activeRecorder = &recorder;
    }

    ~ActiveRecorderGuard()
    {
        if (activeRecorder == &recorder)
            activeRecorder = nullptr;
    }

private:
    RegularTraceRecorder& recorder;
};

TEST(EngineTraceBackendTest, DoesNotAddSecondExclusionOrTraceWhenFilterInitializationDoesNotReturnStorage)
{
    constexpr std::array initResults{FilterInitResult::Null, FilterInitResult::Foreign};

    for (const auto initResult : initResults) {
        RegularEngineTraceContext context;
        RegularTraceRecorder recorder{
            .initResult = initResult,
            .managerStorage = &context.managerHolder,
            .endPositionOffset = &context.endPositionOffset
        };
        ActiveRecorderGuard activeRecorderGuard{recorder};
        EngineTrace trace{context};
        std::byte firstExcluded{};
        std::byte secondExcluded{};

        EXPECT_FALSE(trace.traceHull(makeRequest({&firstExcluded, &secondExcluded})).hasValue());
        EXPECT_EQ(recorder.initCalls, 1);
        EXPECT_EQ(recorder.addSecondExclusionCalls, 0);
        EXPECT_EQ(recorder.traceCalls, 0);
    }
}

TEST(EngineTraceBackendTest, UsesOneBindingSnapshotForTheWholeTraceCall)
{
    RegularEngineTraceContext context;
    RegularTraceRecorder recorder{
        .changeOutputOffsetsOnInit = true,
        .managerStorage = &context.managerHolder,
        .endPositionOffset = &context.endPositionOffset
    };
    ActiveRecorderGuard activeRecorderGuard{recorder};
    EngineTrace trace{context};

    const auto result = trace.traceHull(makeRequest());

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(context.patternSearchResultsCalls, 1);
    EXPECT_EQ(context.patternGetCalls, 8);
    EXPECT_EQ(recorder.initCalls, 1);
    EXPECT_EQ(recorder.traceCalls, 1);
}

TEST(EngineTraceBackendTest, FailsClosedWhenManagerBecomesNullBeforeTraceInvocation)
{
    RegularEngineTraceContext context;
    RegularTraceRecorder recorder{
        .clearManagerOnInit = true,
        .managerStorage = &context.managerHolder,
        .endPositionOffset = &context.endPositionOffset
    };
    ActiveRecorderGuard activeRecorderGuard{recorder};
    EngineTrace trace{context};

    EXPECT_FALSE(trace.traceHull(makeRequest()).hasValue());
    EXPECT_EQ(recorder.initCalls, 1);
    EXPECT_EQ(recorder.traceCalls, 0);
}

TEST(TraceOutputDecoderTest, PreservesHitAndFreeFlightOutputInterpretation)
{
    engine_trace::TraceOutputLayout layout{
        .endPositionOffset = 0x10,
        .normalOffset = 0x20,
        .fractionOffset = 0x30,
        .rawEntityHandleOffset = 0x40
    };
    cs2::engine_trace::TraceOutputStorage output{};
    writeOutput(output, 0x10, cs2::Vector{1.0f, 2.0f, 3.0f});
    writeOutput(output, 0x20, cs2::Vector{0.0f, 0.0f, 1.0f});
    writeOutput(output, 0x30, 0.5f);
    writeOutput(output, 0x40, std::int32_t{42});

    const auto hit = engine_trace::decodeTraceOutput(output, layout);

    ASSERT_TRUE(hit.hasValue());
    EXPECT_EQ(hit.value().rawEntityHandle.value(), 42);

    writeOutput(output, 0x20, cs2::Vector{});
    writeOutput(output, 0x30, 1.0f);
    const auto freeFlight = engine_trace::decodeTraceOutput(output, layout);

    ASSERT_TRUE(freeFlight.hasValue());
    EXPECT_FALSE(freeFlight.value().rawEntityHandle.hasValue());

    writeOutput(output, 0x30, 0.5f);
    EXPECT_FALSE(engine_trace::decodeTraceOutput(output, layout).hasValue());
}

}
