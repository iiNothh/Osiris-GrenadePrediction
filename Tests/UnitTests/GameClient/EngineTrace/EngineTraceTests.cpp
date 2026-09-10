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

[[nodiscard]] constexpr engine_trace::HullTraceRequest makeRequest(cs2::Vector start, cs2::Vector end,
    engine_trace::TraceFilterExcludedEntities excludedEntities = {}) noexcept
{
    return {
        .start = start,
        .end = end,
        .mins = {-2.0f, -2.0f, -2.0f},
        .maxs = {2.0f, 2.0f, 2.0f},
        .excludedEntities = excludedEntities,
        .filter = {.interactsWith = cs2::engine_trace::kSolidMask, .collisionGroup = cs2::CollisionGroup::Default,
            .queryFlags = cs2::PhysicsQueryFlag::IncludeSolidContacts | cs2::PhysicsQueryFlag::RespectDisabledSolidContacts
                | cs2::PhysicsQueryFlag::IncludeTriggerContacts}
    };
}

struct GenericTraceRecorder {
    int filterConstructionCalls{};
    int addSecondExclusionCalls{};
    int buildQueryShapeCalls{};
    int traceShapeCalls{};
    void* firstExcludedEntity{};
    cs2::engine_trace::InteractionLayer interactsWith{};
    cs2::CollisionGroup collisionGroup{};
    cs2::PhysicsQueryFlag queryFlags{};
    void* secondExclusionFirstEntity{};
    void* secondExcludedEntity{};
    cs2::AABB_t bounds{};
    cs2::Vector start{};
    cs2::Vector end{};
    bool traceShapeReceivedBuiltQueryShape{};
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

[[nodiscard]] cs2::CTraceFilter* genericConstructFilter(cs2::CTraceFilter* storage, void* firstExcludedEntity, cs2::engine_trace::InteractionLayer interactsWith,
    cs2::CollisionGroup collisionGroup, cs2::PhysicsQueryFlag queryFlags) noexcept
{
    if (activeRecorder != nullptr) {
        ++activeRecorder->filterConstructionCalls;
        activeRecorder->firstExcludedEntity = firstExcludedEntity;
        activeRecorder->interactsWith = interactsWith;
        activeRecorder->collisionGroup = collisionGroup;
        activeRecorder->queryFlags = queryFlags;
    }
    return storage;
}

void genericAddSecondExcludedEntity(cs2::CTraceFilter*, void* firstExcludedEntity, void* secondExcludedEntity) noexcept
{
    if (activeRecorder != nullptr) {
        ++activeRecorder->addSecondExclusionCalls;
        activeRecorder->secondExclusionFirstEntity = firstExcludedEntity;
        activeRecorder->secondExcludedEntity = secondExcludedEntity;
    }
}

void genericBuildQueryShape(cs2::RnQueryShapeAttr_t* queryShape, const cs2::AABB_t* bounds) noexcept
{
    if (activeRecorder != nullptr) {
        ++activeRecorder->buildQueryShapeCalls;
        activeRecorder->bounds = *bounds;
        queryShape->storage[0] = std::byte{0x3C};
        queryShape->storage[sizeof(queryShape->storage) - 1] = std::byte{0xA5};
    }
}

template <typename T>
void writeOutput(cs2::CGameTrace& output, std::size_t offset, T value) noexcept
{
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    for (std::size_t i{}; i < sizeof(T); ++i)
        output.storage[offset + i] = bytes[i];
}

[[nodiscard]] bool genericTraceShape(cs2::PhysicsWorldPointerSlot, const cs2::RnQueryShapeAttr_t* queryShape, const cs2::Vector* start, const cs2::Vector* end,
    cs2::CTraceFilter*, cs2::CGameTrace* output) noexcept
{
    if (activeRecorder != nullptr) {
        ++activeRecorder->traceShapeCalls;
        activeRecorder->start = *start;
        activeRecorder->end = *end;
        activeRecorder->traceShapeReceivedBuiltQueryShape = queryShape->storage[0] == std::byte{0x3C}
            && queryShape->storage[sizeof(queryShape->storage) - 1] == std::byte{0xA5};
    }
    auto& traceOutput = *output;
    writeOutput(traceOutput, 0x10, cs2::Vector{1.0f, 2.0f, 3.0f});
    writeOutput(traceOutput, 0x20, cs2::Vector{});
    writeOutput(traceOutput, 0x30, 1.0f);
    return true;
}

enum class GenericTraceDependency {
    TraceShape,
    QueryShapeBuilder,
    PhysicsWorldPointerSlotStorage,
    PhysicsWorldPointerSlot,
    FilterConstruction,
    SecondExclusion,
    EndPositionOffset,
    NormalOffset,
    FractionOffset
};

struct GenericEngineTraceContext {
    struct PatternSearchResults {
        template <typename T>
        [[nodiscard]] auto get() const noexcept
        {
            if constexpr (std::is_same_v<T, TraceShapeFunctionPointer>)
                return context.traceShapeAvailable ? &genericTraceShape : nullptr;
            else if constexpr (std::is_same_v<T, BuildRnQueryShapeAttrFromAABBFunctionPointer>)
                return context.buildQueryShapeAvailable ? &genericBuildQueryShape : nullptr;
            else if constexpr (std::is_same_v<T, PhysicsWorldPointerSlotStoragePointer>)
                return context.physicsWorldPointerSlotStorageAvailable ? &context.physicsWorldPointerSlot : nullptr;
            else if constexpr (std::is_same_v<T, CTraceFilterConstructionFunctionPointer>)
                return context.filterConstructionAvailable ? &genericConstructFilter : nullptr;
            else if constexpr (std::is_same_v<T, CTraceFilterAddExcludedEntityFunctionPointer>)
                return context.secondExclusionAvailable ? &genericAddSecondExcludedEntity : nullptr;
            else if constexpr (std::is_same_v<T, CGameTraceEndPositionOffset>)
                return CGameTraceOffset<cs2::Vector, std::int32_t>{context.endPositionOffset};
            else if constexpr (std::is_same_v<T, CGameTraceNormalOffset>)
                return CGameTraceOffset<cs2::Vector, std::int32_t>{context.normalOffset};
            else if constexpr (std::is_same_v<T, CGameTraceFractionOffset>)
                return CGameTraceOffset<float, std::int32_t>{context.fractionOffset};
            else {
                static_assert(std::is_same_v<T, CGameTraceRawEntityHandleOffset>);
                return CGameTraceOffset<std::int32_t, std::uint8_t>{};
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

    void setDependencyAvailable(GenericTraceDependency dependency, bool available) noexcept
    {
        switch (dependency) {
        case GenericTraceDependency::TraceShape: traceShapeAvailable = available; break;
        case GenericTraceDependency::QueryShapeBuilder: buildQueryShapeAvailable = available; break;
        case GenericTraceDependency::PhysicsWorldPointerSlotStorage: physicsWorldPointerSlotStorageAvailable = available; break;
        case GenericTraceDependency::PhysicsWorldPointerSlot: physicsWorldPointerSlot = available ? &physicsWorld : nullptr; break;
        case GenericTraceDependency::FilterConstruction: filterConstructionAvailable = available; break;
        case GenericTraceDependency::SecondExclusion: secondExclusionAvailable = available; break;
        case GenericTraceDependency::EndPositionOffset: endPositionOffset = available ? 0x10 : 0; break;
        case GenericTraceDependency::NormalOffset: normalOffset = available ? 0x20 : 0; break;
        case GenericTraceDependency::FractionOffset: fractionOffset = available ? 0x30 : 0; break;
        }
    }

    std::byte physicsWorldObject{};
    cs2::IVPhysics2World* physicsWorld{reinterpret_cast<cs2::IVPhysics2World*>(&physicsWorldObject)};
    cs2::PhysicsWorldPointerSlot physicsWorldPointerSlot{&physicsWorld};
    bool traceShapeAvailable{true};
    bool buildQueryShapeAvailable{true};
    bool physicsWorldPointerSlotStorageAvailable{true};
    bool filterConstructionAvailable{true};
    bool secondExclusionAvailable{true};
    std::int32_t endPositionOffset{0x10};
    std::int32_t normalOffset{0x20};
    std::int32_t fractionOffset{0x30};
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
    EXPECT_EQ(recorder.filterConstructionCalls, 0);
    EXPECT_EQ(recorder.buildQueryShapeCalls, 0);
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceTest, RejectsInvalidGenericHullBoundsBeforeNativeCalls)
{
    constexpr auto nan = std::bit_cast<float>(std::uint32_t{0x7FC00000});
    constexpr auto infinity = std::bit_cast<float>(std::uint32_t{0x7F800000});
    struct InvalidHullBounds {
        cs2::Vector mins;
        cs2::Vector maxs;
    };
    constexpr std::array invalidBounds{
        InvalidHullBounds{{nan, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f}},
        InvalidHullBounds{{-2.0f, nan, -2.0f}, {2.0f, 2.0f, 2.0f}},
        InvalidHullBounds{{-2.0f, -2.0f, nan}, {2.0f, 2.0f, 2.0f}},
        InvalidHullBounds{{-2.0f, -2.0f, -2.0f}, {infinity, 2.0f, 2.0f}},
        InvalidHullBounds{{-2.0f, -2.0f, -2.0f}, {2.0f, infinity, 2.0f}},
        InvalidHullBounds{{-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, infinity}},
        InvalidHullBounds{{3.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f}},
        InvalidHullBounds{{-2.0f, 3.0f, -2.0f}, {2.0f, 2.0f, 2.0f}},
        InvalidHullBounds{{-2.0f, -2.0f, 3.0f}, {2.0f, 2.0f, 2.0f}}
    };

    for (const auto& bounds : invalidBounds) {
        GenericEngineTraceContext context;
        GenericTraceRecorder recorder;
        ActiveRecorderGuard activeRecorderGuard{recorder};
        EngineTrace trace{context};
        auto request = makeRequest({}, {});
        request.mins = bounds.mins;
        request.maxs = bounds.maxs;

        EXPECT_FALSE(trace.traceHull(request).hasValue());
        EXPECT_EQ(context.patternSearchResultsCalls, 0);
        EXPECT_EQ(recorder.filterConstructionCalls, 0);
        EXPECT_EQ(recorder.buildQueryShapeCalls, 0);
        EXPECT_EQ(recorder.traceShapeCalls, 0);
    }
}

TEST(EngineTraceTest, FailsClosedWithoutRequiredGenericDependencies)
{
    constexpr std::array dependencies{
        GenericTraceDependency::TraceShape,
        GenericTraceDependency::QueryShapeBuilder,
        GenericTraceDependency::PhysicsWorldPointerSlotStorage,
        GenericTraceDependency::PhysicsWorldPointerSlot,
        GenericTraceDependency::FilterConstruction,
        GenericTraceDependency::EndPositionOffset,
        GenericTraceDependency::NormalOffset,
        GenericTraceDependency::FractionOffset
    };

    for (const auto dependency : dependencies) {
        GenericEngineTraceContext context;
        GenericTraceRecorder recorder;
        ActiveRecorderGuard activeRecorderGuard{recorder};
        context.setDependencyAvailable(dependency, false);
        EngineTrace trace{context};

        EXPECT_FALSE(trace.traceHull(makeRequest({}, {})).hasValue());
        EXPECT_EQ(recorder.filterConstructionCalls, 0);
        EXPECT_EQ(recorder.addSecondExclusionCalls, 0);
        EXPECT_EQ(recorder.buildQueryShapeCalls, 0);
        EXPECT_EQ(recorder.traceShapeCalls, 0);
    }
}

TEST(EngineTraceTest, RequiresSecondExclusionBindingOnlyWhenSecondEntityExists)
{
    {
        GenericEngineTraceContext context;
        GenericTraceRecorder recorder;
        ActiveRecorderGuard activeRecorderGuard{recorder};
        context.setDependencyAvailable(GenericTraceDependency::SecondExclusion, false);
        EngineTrace trace{context};

        EXPECT_TRUE(trace.traceHull(makeRequest({}, {})).hasValue());
        EXPECT_EQ(recorder.filterConstructionCalls, 1);
        EXPECT_EQ(recorder.addSecondExclusionCalls, 0);
        EXPECT_EQ(recorder.buildQueryShapeCalls, 1);
        EXPECT_EQ(recorder.traceShapeCalls, 1);
    }

    {
        GenericEngineTraceContext context;
        GenericTraceRecorder recorder;
        ActiveRecorderGuard activeRecorderGuard{recorder};
        context.setDependencyAvailable(GenericTraceDependency::SecondExclusion, false);
        EngineTrace trace{context};
        std::byte firstExcluded{};
        std::byte secondExcluded{};

        EXPECT_FALSE(trace.traceHull(makeRequest({}, {}, {&firstExcluded, &secondExcluded})).hasValue());
        EXPECT_EQ(recorder.filterConstructionCalls, 0);
        EXPECT_EQ(recorder.addSecondExclusionCalls, 0);
        EXPECT_EQ(recorder.buildQueryShapeCalls, 0);
        EXPECT_EQ(recorder.traceShapeCalls, 0);
    }
}

TEST(EngineTraceTest, GenericTracingDoesNotRequireGrenadeOrRawEntityHandleLayoutPatterns)
{
    GenericEngineTraceContext context;
    GenericTraceRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    EngineTrace trace{context};

    const auto result = trace.traceHull(makeRequest({}, {}));

    ASSERT_TRUE(result.hasValue());
    EXPECT_FALSE(result.value().rawEntityHandle.hasValue());
    EXPECT_EQ(recorder.filterConstructionCalls, 1);
    EXPECT_EQ(recorder.collisionGroup, cs2::CollisionGroup::Default);
    EXPECT_EQ(recorder.buildQueryShapeCalls, 1);
    EXPECT_EQ(recorder.bounds.m_vMinBounds, (cs2::Vector{-2.0f, -2.0f, -2.0f}));
    EXPECT_EQ(recorder.bounds.m_vMaxBounds, (cs2::Vector{2.0f, 2.0f, 2.0f}));
    EXPECT_EQ(recorder.traceShapeCalls, 1);
    EXPECT_TRUE(recorder.traceShapeReceivedBuiltQueryShape);
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

}
