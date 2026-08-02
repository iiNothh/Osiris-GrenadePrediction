#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include <GameClient/EngineTrace/EngineTrace.h>

#if defined(_WIN64)
namespace
{

struct CallState {
    bool shapeBuilderCalled{};
    bool descriptorWasZero{};
    engine_trace::Bounds6f bounds{};
    bool filterCalled{};
    bool filterWasZero{};
    void* skipEntity{};
    std::uint64_t mask{};
    std::uint8_t collisionGroup{};
    std::uint8_t queryByte{};
    bool traceCalled{};
    void* manager{};
    cs2::Vector start{};
    cs2::Vector end{};
    float fraction{1.0f};
    cs2::Vector endPosition{1.0f, 2.0f, 3.0f};
    cs2::Vector normal{};
    std::int32_t endPositionOffset{32};
    std::int32_t normalOffset{48};
    std::int32_t fractionOffset{16};
};

CallState callState{};
void* manager{};

template <typename T>
void writeOutput(engine_trace::OutputStorage* output, std::size_t offset, T value) noexcept
{
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    for (std::size_t i = 0; i < sizeof(T); ++i)
        output->storage[offset + i] = bytes[i];
}

void shapeBuilder(void* descriptor, const engine_trace::Bounds6f* bounds) noexcept
{
    callState.shapeBuilderCalled = true;
    callState.descriptorWasZero = true;
    const auto* storage = static_cast<const std::byte*>(descriptor);
    for (std::size_t i = 0; i < engine_trace::kDescriptorCapacity; ++i)
        callState.descriptorWasZero = callState.descriptorWasZero && storage[i] == std::byte{};
    callState.bounds = *bounds;
}

void* initFilter(void* filter, void* skipEntity, std::uint64_t mask, std::uint8_t collisionGroup, std::uint8_t queryByte) noexcept
{
    callState.filterCalled = true;
    callState.filterWasZero = true;
    const auto* storage = static_cast<const std::byte*>(filter);
    for (std::size_t i = 0; i < engine_trace::kFilterCapacity; ++i)
        callState.filterWasZero = callState.filterWasZero && storage[i] == std::byte{};
    callState.skipEntity = skipEntity;
    callState.mask = mask;
    callState.collisionGroup = collisionGroup;
    callState.queryByte = queryByte;
    return filter;
}

bool traceShape(void* managerHolder, const void*, const cs2::Vector* start, const cs2::Vector* end, void*, void* output) noexcept
{
    callState.traceCalled = true;
    callState.manager = managerHolder;
    callState.start = *start;
    callState.end = *end;
    auto* traceOutput = static_cast<engine_trace::OutputStorage*>(output);
    writeOutput(traceOutput, static_cast<std::size_t>(callState.fractionOffset), callState.fraction);
    writeOutput(traceOutput, static_cast<std::size_t>(callState.endPositionOffset), callState.endPosition);
    writeOutput(traceOutput, static_cast<std::size_t>(callState.normalOffset), callState.normal);
    return true;
}

struct PatternResults {
    bool provideShapeBuilder{true};
    bool provideTraceShape{true};
    bool provideManagerStorage{true};
    bool provideFilter{true};

    template <typename T>
    auto get() const noexcept
    {
        if constexpr (std::is_same_v<T, ShapeBuilderFunctionPointer>)
            return provideShapeBuilder ? &shapeBuilder : nullptr;
        else if constexpr (std::is_same_v<T, TraceShapeFunctionPointer>)
            return provideTraceShape ? &traceShape : nullptr;
        else if constexpr (std::is_same_v<T, GameTraceManagerStoragePointer>)
            return provideManagerStorage ? &manager : nullptr;
        else if constexpr (std::is_same_v<T, InitFilterFunctionPointer>)
            return provideFilter ? &initFilter : nullptr;
        else if constexpr (std::is_same_v<T, CGameTraceEndPositionOffset>)
            return callState.endPositionOffset;
        else if constexpr (std::is_same_v<T, CGameTraceNormalOffset>)
            return callState.normalOffset;
        else
            return callState.fractionOffset;
    }
};

struct HookContext {
    [[nodiscard]] const PatternResults& patternSearchResults() const noexcept
    {
        return results;
    }

    PatternResults results{};
};

class EngineTraceTest : public testing::Test {
protected:
    void SetUp() override
    {
        callState = {};
        callState.fraction = 1.0f;
        callState.endPosition = {1.0f, 2.0f, 3.0f};
        manager = &callState;
    }
};

TEST_F(EngineTraceTest, BuildsAndExecutesGrenadeHullTrace)
{
    HookContext hookContext;
    EngineTrace<HookContext> engineTrace{hookContext};
    void* const skipEntity = reinterpret_cast<void*>(0x1234);

    const auto result = engineTrace.traceGrenadeHull({10.0f, 20.0f, 30.0f}, {40.0f, 50.0f, 60.0f}, skipEntity);

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().fraction, 1.0f);
    EXPECT_EQ(result.value().endPos, (cs2::Vector{1.0f, 2.0f, 3.0f}));
    EXPECT_EQ(callState.shapeBuilderCalled, true);
    EXPECT_EQ(callState.descriptorWasZero, true);
    EXPECT_EQ(callState.bounds.mins, (cs2::Vector{-2.0f, -2.0f, -2.0f}));
    EXPECT_EQ(callState.bounds.maxs, (cs2::Vector{2.0f, 2.0f, 2.0f}));
    EXPECT_EQ(callState.filterCalled, true);
    EXPECT_EQ(callState.filterWasZero, true);
    EXPECT_EQ(callState.skipEntity, skipEntity);
    EXPECT_EQ(callState.mask, MASK_GRENADE);
    EXPECT_EQ(callState.collisionGroup, 4);
    EXPECT_EQ(callState.queryByte, 7);
    EXPECT_EQ(callState.traceCalled, true);
    EXPECT_EQ(callState.manager, &callState);
    EXPECT_EQ(callState.start, (cs2::Vector{10.0f, 20.0f, 30.0f}));
    EXPECT_EQ(callState.end, (cs2::Vector{40.0f, 50.0f, 60.0f}));
}

TEST_F(EngineTraceTest, FailsClosedForInvalidPrerequisitesOrOutput)
{
    HookContext hookContext;
    EngineTrace<HookContext> engineTrace{hookContext};

    hookContext.results.provideShapeBuilder = false;
    EXPECT_FALSE(engineTrace.traceGrenadeHull({}, {}, nullptr).hasValue());
    hookContext.results.provideShapeBuilder = true;
    hookContext.results.provideTraceShape = false;
    EXPECT_FALSE(engineTrace.traceGrenadeHull({}, {}, nullptr).hasValue());
    hookContext.results.provideTraceShape = true;
    hookContext.results.provideManagerStorage = false;
    EXPECT_FALSE(engineTrace.traceGrenadeHull({}, {}, nullptr).hasValue());
    hookContext.results.provideManagerStorage = true;
    hookContext.results.provideFilter = false;
    EXPECT_FALSE(engineTrace.traceGrenadeHull({}, {}, nullptr).hasValue());
    hookContext.results.provideFilter = true;
    manager = nullptr;
    EXPECT_FALSE(engineTrace.traceGrenadeHull({}, {}, nullptr).hasValue());
    manager = &callState;
    callState.endPositionOffset = 2;
    EXPECT_FALSE(engineTrace.traceGrenadeHull({}, {}, nullptr).hasValue());
    callState.endPositionOffset = 32;
    callState.normalOffset = 32;
    EXPECT_FALSE(engineTrace.traceGrenadeHull({}, {}, nullptr).hasValue());
    callState.normalOffset = 48;
    callState.fraction = 0.5f;
    EXPECT_FALSE(engineTrace.traceGrenadeHull({}, {}, nullptr).hasValue());
    callState.normal = {0.0f, 0.0f, 1.0f};
    EXPECT_TRUE(engineTrace.traceGrenadeHull({}, {}, nullptr).hasValue());
}

}
#endif
