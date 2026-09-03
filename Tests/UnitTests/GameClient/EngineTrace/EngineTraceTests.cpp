#include <array>
#include <bit>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include <GameClient/EngineTrace/EngineTrace.h>

namespace
{

[[nodiscard]] constexpr engine_trace::HullTraceRequest makeRequest(cs2::Vector start, cs2::Vector end,
    engine_trace::TraceFilterExcludedEntities excludedEntities = {}) noexcept
{
    return {
        .start = start,
        .end = end,
        .mins = {-2.0f, -2.0f, -2.0f},
        .maxs = {2.0f, 2.0f, 2.0f},
        .excludedEntities = excludedEntities,
        .filter = {.mask = engine_trace::kMaskGrenade, .collisionGroup = 4, .queryByte = 7}
    };
}

struct GenericTraceRecorder {
    int initFilterCalls{};
    int traceShapeCalls{};
};

GenericTraceRecorder* activeRecorder{};

class ActiveRecorderGuard {
public:
    explicit ActiveRecorderGuard(GenericTraceRecorder& recorder) noexcept
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
    GenericTraceRecorder& recorder;
};

[[nodiscard]] void* genericInitFilter(void*, void*, std::uint64_t, std::uint8_t, std::uint8_t) noexcept
{
    if (activeRecorder != nullptr)
        ++activeRecorder->initFilterCalls;
    return nullptr;
}

[[nodiscard]] bool genericTraceShape(void*, const void*, const cs2::Vector*, const cs2::Vector*, void*, void*) noexcept
{
    if (activeRecorder != nullptr)
        ++activeRecorder->traceShapeCalls;
    return false;
}

struct GenericEngineTraceContext {
    struct PatternSearchResults {
        template <typename T>
        [[nodiscard]] static consteval bool supports() noexcept
        {
            return std::is_same_v<T, TraceShapeFunctionPointer>
                || std::is_same_v<T, GameTraceManagerStoragePointer>
                || std::is_same_v<T, InitFilterFunctionPointer>
                || std::is_same_v<T, CGameTraceEndPositionOffset>
                || std::is_same_v<T, CGameTraceNormalOffset>
                || std::is_same_v<T, CGameTraceFractionOffset>;
        }

        template <typename T>
        [[nodiscard]] auto get() const noexcept
        {
            if constexpr (std::is_same_v<T, TraceShapeFunctionPointer>)
                return &genericTraceShape;
            else if constexpr (std::is_same_v<T, GameTraceManagerStoragePointer>)
                return &context.managerHolder;
            else if constexpr (std::is_same_v<T, InitFilterFunctionPointer>)
                return &genericInitFilter;
            else if constexpr (std::is_same_v<T, CGameTraceEndPositionOffset>)
                return std::int32_t{0x10};
            else if constexpr (std::is_same_v<T, CGameTraceNormalOffset>)
                return std::int32_t{0x20};
            else {
                static_assert(std::is_same_v<T, CGameTraceFractionOffset>);
                return std::int32_t{0x30};
            }
        }

        GenericEngineTraceContext& context;
    };

    GenericEngineTraceContext() noexcept
        : results{*this}
    {
    }

    [[nodiscard]] const PatternSearchResults& patternSearchResults() const noexcept
    {
        ++patternSearchResultsCalls;
        return results;
    }

    void* managerHolder{};
    mutable int patternSearchResultsCalls{};
    PatternSearchResults results;
};

TEST(EngineTraceTest, RejectsNonFiniteGenericInputsBeforeNativeCalls)
{
    constexpr auto nan = std::bit_cast<float>(std::uint32_t{0x7FC00000});
    constexpr auto infinity = std::bit_cast<float>(std::uint32_t{0x7F800000});
    constexpr std::array invalidVectors{
        cs2::Vector{nan, 0.0f, 0.0f},
        cs2::Vector{infinity, 0.0f, 0.0f}
    };

    GenericEngineTraceContext context;
    GenericTraceRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    EngineTrace trace{context};

    for (const auto invalid : invalidVectors) {
        EXPECT_FALSE(trace.traceHull(makeRequest(invalid, {})).hasValue());
        EXPECT_FALSE(trace.traceHull(makeRequest({}, invalid)).hasValue());
    }

    EXPECT_EQ(context.patternSearchResultsCalls, 0);
    EXPECT_EQ(recorder.initFilterCalls, 0);
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(HullTraceRequestTest, NormalizesEntityExclusionsWithoutChangingTheSecondExclusionSlot)
{
    std::byte first{};
    std::byte second{};

    const engine_trace::TraceFilterExcludedEntities noEntities;
    const engine_trace::TraceFilterExcludedEntities oneEntity{nullptr, &first};
    const engine_trace::TraceFilterExcludedEntities twoEntities{&first, &second};
    const engine_trace::TraceFilterExcludedEntities duplicateEntity{&first, &first};

    EXPECT_EQ(noEntities.first, nullptr);
    EXPECT_EQ(noEntities.second, nullptr);
    EXPECT_EQ(oneEntity.first, nullptr);
    EXPECT_EQ(oneEntity.second, &first);
    EXPECT_EQ(twoEntities.first, &first);
    EXPECT_EQ(twoEntities.second, &second);
    EXPECT_EQ(duplicateEntity.first, &first);
    EXPECT_EQ(duplicateEntity.second, nullptr);
}

TEST(HullTraceRequestTest, MapsSemanticRequestToRawDescriptor)
{
    const auto request = makeRequest({1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f});

    const auto descriptor = engine_trace::makeHullTraceDescriptor(request);

    EXPECT_EQ(descriptor.mins, request.mins);
    EXPECT_EQ(descriptor.maxs, request.maxs);
    EXPECT_EQ(descriptor.type, 2u);
    EXPECT_EQ(descriptor.zeroesBeforeType[0], std::byte{});
    EXPECT_EQ(descriptor.trailingZeroes[0], std::byte{});
}

}
