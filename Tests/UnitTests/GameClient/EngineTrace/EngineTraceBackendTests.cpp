#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include <CS2/Constants/CollisionGroup.h>
#include <CS2/Constants/InteractionLayers.h>
#include <CS2/Constants/PhysicsQueryFlag.h>
#include <GameClient/EngineTrace/EngineTrace.h>
#include <GameClient/EngineTrace/HullTraceRequest.h>
#include <GameClient/EngineTrace/TraceFilter.h>

namespace
{

enum class FilterConstructionResult {
    Storage,
    Null,
    Foreign
};

struct RegularTraceRecorder {
    FilterConstructionResult constructionResult{FilterConstructionResult::Storage};
    bool clearManagerOnFilterConstruction{};
    bool changeOutputOffsetsOnFilterConstruction{};
    cs2::PhysicsWorldPointerSlotStorage physicsWorldPointerSlotStorage{};
    std::int32_t* endPositionOffset{};
    int filterConstructionCalls{};
    int addSecondExclusionCalls{};
    int traceCalls{};
};

RegularTraceRecorder* activeRecorder{};

[[nodiscard]] cs2::CTraceFilter* regularConstructFilter(cs2::CTraceFilter* storage, void*, cs2::engine_trace::InteractionLayer,
    cs2::CollisionGroup, cs2::PhysicsQueryFlag) noexcept
{
    if (activeRecorder == nullptr)
        return nullptr;

    ++activeRecorder->filterConstructionCalls;
    if (activeRecorder->clearManagerOnFilterConstruction)
        *activeRecorder->physicsWorldPointerSlotStorage = nullptr;
    if (activeRecorder->changeOutputOffsetsOnFilterConstruction)
        *activeRecorder->endPositionOffset = 0;
    if (activeRecorder->constructionResult == FilterConstructionResult::Null)
        return nullptr;
    if (activeRecorder->constructionResult == FilterConstructionResult::Foreign)
        return reinterpret_cast<cs2::CTraceFilter*>(storage->storage + 1);
    return storage;
}

void regularAddSecondExcludedEntity(cs2::CTraceFilter*, void*, void*) noexcept
{
    if (activeRecorder != nullptr)
        ++activeRecorder->addSecondExclusionCalls;
}

void regularBuildQueryShape(cs2::RnQueryShapeAttr_t*, const cs2::AABB_t*) noexcept
{
}

template <typename T>
void writeOutput(cs2::CGameTrace& output, std::size_t offset, T value) noexcept
{
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    for (std::size_t i{}; i < sizeof(T); ++i)
        output.storage[offset + i] = bytes[i];
}

[[nodiscard]] bool regularTraceShape(cs2::PhysicsWorldPointerSlot, const cs2::RnQueryShapeAttr_t*, const cs2::Vector*, const cs2::Vector*, cs2::CTraceFilter*, cs2::CGameTrace* output) noexcept
{
    if (activeRecorder == nullptr)
        return false;

    ++activeRecorder->traceCalls;
    auto& traceOutput = *output;
    writeOutput(traceOutput, 0x10, cs2::Vector{1.0f, 2.0f, 3.0f});
    writeOutput(traceOutput, 0x20, cs2::Vector{});
    writeOutput(traceOutput, 0x30, 1.0f);
    return true;
}

struct RegularEngineTraceContext {
    struct PatternSearchResults {
        template <typename T>
        [[nodiscard]] auto get() const noexcept
        {
            ++context.patternGetCalls;
            if constexpr (std::is_same_v<T, TraceShapeFunctionPointer>)
                return &regularTraceShape;
            else if constexpr (std::is_same_v<T, BuildRnQueryShapeAttrFromAABBFunctionPointer>)
                return &regularBuildQueryShape;
            else if constexpr (std::is_same_v<T, PhysicsWorldPointerSlotStoragePointer>)
                return &context.physicsWorldPointerSlot;
            else if constexpr (std::is_same_v<T, CTraceFilterConstructionFunctionPointer>)
                return &regularConstructFilter;
            else if constexpr (std::is_same_v<T, CTraceFilterAddExcludedEntityFunctionPointer>)
                return &regularAddSecondExcludedEntity;
            else if constexpr (std::is_same_v<T, CGameTraceEndPositionOffset>)
                return CGameTraceOffset<cs2::Vector, std::int32_t>{context.endPositionOffset};
            else if constexpr (std::is_same_v<T, CGameTraceNormalOffset>)
                return CGameTraceOffset<cs2::Vector, std::int32_t>{context.normalOffset};
            else if constexpr (std::is_same_v<T, CGameTraceFractionOffset>)
                return CGameTraceOffset<float, std::int32_t>{context.fractionOffset};
            else {
                static_assert(std::is_same_v<T, CGameTraceRawEntityHandleOffset>);
                return CGameTraceOffset<std::int32_t, std::uint8_t>{context.rawEntityHandleOffset};
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

    std::byte physicsWorldObject{};
    cs2::IVPhysics2World* physicsWorld{reinterpret_cast<cs2::IVPhysics2World*>(&physicsWorldObject)};
    cs2::PhysicsWorldPointerSlot physicsWorldPointerSlot{&physicsWorld};
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
        .filter = {.interactsWith = cs2::engine_trace::kSolidMask, .collisionGroup = cs2::CollisionGroup::Default,
            .queryFlags = cs2::PhysicsQueryFlag::IncludeSolidContacts | cs2::PhysicsQueryFlag::RespectDisabledSolidContacts
                | cs2::PhysicsQueryFlag::IncludeTriggerContacts}
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

TEST(EngineTraceBackendTest, DoesNotAddSecondExclusionOrTraceWhenFilterConstructionDoesNotReturnStorage)
{
    constexpr std::array constructionResults{FilterConstructionResult::Null, FilterConstructionResult::Foreign};

    for (const auto constructionResult : constructionResults) {
        RegularEngineTraceContext context;
        RegularTraceRecorder recorder{
            .constructionResult = constructionResult,
            .physicsWorldPointerSlotStorage = &context.physicsWorldPointerSlot,
            .endPositionOffset = &context.endPositionOffset
        };
        ActiveRecorderGuard activeRecorderGuard{recorder};
        EngineTrace trace{context};
        std::byte firstExcluded{};
        std::byte secondExcluded{};

        EXPECT_FALSE(trace.traceHull(makeRequest({&firstExcluded, &secondExcluded})).hasValue());
        EXPECT_EQ(recorder.filterConstructionCalls, 1);
        EXPECT_EQ(recorder.addSecondExclusionCalls, 0);
        EXPECT_EQ(recorder.traceCalls, 0);
    }
}

TEST(EngineTraceBackendTest, UsesOneBindingSnapshotForTheWholeTraceCall)
{
    RegularEngineTraceContext context;
    RegularTraceRecorder recorder{
        .changeOutputOffsetsOnFilterConstruction = true,
        .physicsWorldPointerSlotStorage = &context.physicsWorldPointerSlot,
        .endPositionOffset = &context.endPositionOffset
    };
    ActiveRecorderGuard activeRecorderGuard{recorder};
    EngineTrace trace{context};

    const auto result = trace.traceHull(makeRequest());

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(context.patternSearchResultsCalls, 1);
    EXPECT_EQ(context.patternGetCalls, 9);
    EXPECT_EQ(recorder.filterConstructionCalls, 1);
    EXPECT_EQ(recorder.traceCalls, 1);
}

TEST(EngineTraceBackendTest, FailsClosedWhenManagerBecomesNullBeforeTraceInvocation)
{
    RegularEngineTraceContext context;
    RegularTraceRecorder recorder{
        .clearManagerOnFilterConstruction = true,
        .physicsWorldPointerSlotStorage = &context.physicsWorldPointerSlot,
        .endPositionOffset = &context.endPositionOffset
    };
    ActiveRecorderGuard activeRecorderGuard{recorder};
    EngineTrace trace{context};

    EXPECT_FALSE(trace.traceHull(makeRequest()).hasValue());
    EXPECT_EQ(recorder.filterConstructionCalls, 1);
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
    cs2::CGameTrace output{};
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
