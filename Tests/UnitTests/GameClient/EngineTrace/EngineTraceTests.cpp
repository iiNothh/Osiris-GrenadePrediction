#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include <CS2/Constants/EngineTraceContents.h>
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
        .filter = {.mask = cs2::engine_trace::CONTENTS_SOLID, .collisionGroup = 4, .queryByte = 7}
    };
}

struct GenericTraceRecorder {
    int initFilterCalls{};
    int addSecondExclusionCalls{};
    int traceShapeCalls{};
    void* firstExcludedEntity{};
    std::uint64_t filterMask{};
    std::uint8_t collisionGroup{};
    std::uint8_t queryByte{};
    void* secondExclusionFirstEntity{};
    void* secondExcludedEntity{};
    cs2::engine_trace::HullTraceDescriptor descriptor{};
    cs2::Vector start{};
    cs2::Vector end{};
};

TEST(EngineTraceContentsTest, DefinesVerifiedRawCategoriesAndComposites)
{
    EXPECT_EQ(cs2::engine_trace::CONTENTS_EMPTY, 0x00000000ull);
    EXPECT_EQ(cs2::engine_trace::CONTENTS_CATEGORY_SOLID, 0x00000001ull);
    EXPECT_EQ(cs2::engine_trace::CONTENTS_CATEGORY_WATER, 0x00008000ull);
    EXPECT_EQ(cs2::engine_trace::CONTENTS_CATEGORY_STATIC_LEVEL, 0x40000000ull);
    EXPECT_EQ(cs2::engine_trace::CONTENTS_SOLID, 0x000004C1ull);
    EXPECT_EQ(cs2::engine_trace::CONTENTS_SOLID_NO_BLOCK_LOS, 0x00000481ull);
}

TEST(EngineTraceContentsTest, DefinesObservedInteractionMasks)
{
    EXPECT_EQ(cs2::engine_trace::observed_interacts_as::kWall, 0x400004C1ull);
    EXPECT_EQ(cs2::engine_trace::observed_interacts_as::kWindowDynamicProp, 0x10021400ull);
    EXPECT_EQ(cs2::engine_trace::observed_interacts_as::kRailing, 0x40002000ull);
    EXPECT_EQ(cs2::engine_trace::observed_interacts_as::kInteractiveDoor, 0x18020481ull);
    EXPECT_EQ(cs2::engine_trace::observed_interacts_as::kVentCover, 0x100200C1ull);
    EXPECT_EQ(cs2::engine_trace::observed_interacts_as::kDynamicProp, 0x10020081ull);
    EXPECT_EQ(cs2::engine_trace::observed_interacts_as::kBreakableProp, 0x10300081ull);
    EXPECT_EQ(cs2::engine_trace::observed_interacts_as::kLadder, 0x40002100ull);
    EXPECT_EQ(cs2::engine_trace::observed_interacts_as::kPlayerEntity, 0x10060000ull);
}

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

[[nodiscard]] void* genericInitFilter(void* storage, void* firstExcludedEntity, std::uint64_t filterMask,
    std::uint8_t collisionGroup, std::uint8_t queryByte) noexcept
{
    if (activeRecorder != nullptr) {
        ++activeRecorder->initFilterCalls;
        activeRecorder->firstExcludedEntity = firstExcludedEntity;
        activeRecorder->filterMask = filterMask;
        activeRecorder->collisionGroup = collisionGroup;
        activeRecorder->queryByte = queryByte;
    }
    return storage;
}

void genericAddSecondExcludedEntity(void*, void* firstExcludedEntity, void* secondExcludedEntity) noexcept
{
    if (activeRecorder != nullptr) {
        ++activeRecorder->addSecondExclusionCalls;
        activeRecorder->secondExclusionFirstEntity = firstExcludedEntity;
        activeRecorder->secondExcludedEntity = secondExcludedEntity;
    }
}

template <typename T>
void writeOutput(cs2::engine_trace::TraceOutputStorage& output, std::size_t offset, T value) noexcept
{
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    for (std::size_t i{}; i < sizeof(T); ++i)
        output.storage[offset + i] = bytes[i];
}

[[nodiscard]] bool genericTraceShape(void*, const void* descriptor, const cs2::Vector* start, const cs2::Vector* end,
    void*, void* output) noexcept
{
    if (activeRecorder != nullptr) {
        ++activeRecorder->traceShapeCalls;
        activeRecorder->descriptor = *static_cast<const cs2::engine_trace::HullTraceDescriptor*>(descriptor);
        activeRecorder->start = *start;
        activeRecorder->end = *end;
    }
    auto& traceOutput = *static_cast<cs2::engine_trace::TraceOutputStorage*>(output);
    writeOutput(traceOutput, 0x10, cs2::Vector{1.0f, 2.0f, 3.0f});
    writeOutput(traceOutput, 0x20, cs2::Vector{});
    writeOutput(traceOutput, 0x30, 1.0f);
    return true;
}

struct GenericEngineTraceContext {
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
            else if constexpr (std::is_same_v<T, AddSecondExcludedEntityToFilterFunctionPointer>)
                return &genericAddSecondExcludedEntity;
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

    std::byte managerObject{};
    void* managerHolder{&managerObject};
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

TEST(EngineTraceTest, GenericTracingDoesNotRequireNativePipFilterLayoutPatterns)
{
    GenericEngineTraceContext context;
    GenericTraceRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    EngineTrace trace{context};

    const auto result = trace.traceHull(makeRequest({}, {}));

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(recorder.initFilterCalls, 1);
    EXPECT_EQ(recorder.traceShapeCalls, 1);
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

TEST(HullTraceRequestTest, MapsUnequalBoundsToHullDescriptor)
{
    const auto request = makeRequest({1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f});

    const auto descriptor = engine_trace::makeHullTraceDescriptor(request);

    EXPECT_EQ(descriptor.mins, request.mins);
    EXPECT_EQ(descriptor.maxs, request.maxs);
    EXPECT_EQ(descriptor.type, 2u);
    EXPECT_EQ(descriptor.zeroesBeforeType[0], std::byte{});
    EXPECT_EQ(descriptor.trailingZeroes[0], std::byte{});
}

TEST(HullTraceRequestTest, MapsEqualBoundsToLineDescriptor)
{
    constexpr engine_trace::HullTraceRequest request{
        .mins = {1.0f, 2.0f, 3.0f},
        .maxs = {1.0f, 2.0f, 3.0f}
    };

    EXPECT_EQ(engine_trace::makeHullTraceDescriptor(request).type, 0u);
}

TEST(HullTraceRequestTest, UsesExactBoundsEqualityForDescriptorType)
{
    constexpr auto smallestPositiveFloat = std::bit_cast<float>(std::uint32_t{1});
    constexpr engine_trace::HullTraceRequest request{
        .mins = {},
        .maxs = {smallestPositiveFloat, 0.0f, 0.0f}
    };

    EXPECT_EQ(engine_trace::makeHullTraceDescriptor(request).type, 2u);
}

}
