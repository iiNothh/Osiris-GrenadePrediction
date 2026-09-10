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
#include <GameClient/EngineTrace/GrenadeTrace.h>
#include <GameClient/EngineTrace/TraceFilter.h>

namespace
{

constexpr engine_trace::grenade::FilterOverlayLayout kCanonicalFilterOverlayLayout{
    .interactsExcludeOffset = 0x10,
    .interactsAsOffset = 0x18,
    .flagsOffset = 0x39,
    .candidateCollectionModeOffset = 0x40
};

enum class GrenadeConstructionOutcome {
    Correct,
    WrongReturn
};

struct GrenadeTraceRecorder {
    GrenadeConstructionOutcome constructionOutcome{GrenadeConstructionOutcome::Correct};
    cs2::PhysicsWorldPointerSlotStorage physicsWorldPointerSlotStorage{};
    bool clearManagerOnFilterConstruction{};
    int filterConstructionCalls{};
    int secondExclusionCalls{};
    int buildQueryShapeCalls{};
    int traceShapeCalls{};
    bool constructedWithNativeArguments{};
    bool constructorFieldsPreservedBeforeSecondExclusion{};
    bool overlayAppliedBeforeSecondExclusion{};
    bool traceShapeCalledAfterSecondExclusion{};
    cs2::AABB_t bounds{};
    engine_trace::grenade::FilterOverlayLayout filterOverlayLayout{kCanonicalFilterOverlayLayout};
};

GrenadeTraceRecorder* activeRecorder{};

class ActiveRecorderGuard {
public:
    explicit ActiveRecorderGuard(GrenadeTraceRecorder& recorder) noexcept
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
    GrenadeTraceRecorder& recorder;
};

[[nodiscard]] constexpr engine_trace::HullTraceRequest grenadeRequest(cs2::Vector start, cs2::Vector end,
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
void writeOutput(cs2::CGameTrace& output, std::size_t offset, T value) noexcept
{
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    for (std::size_t i{}; i < sizeof(T); ++i)
        output.storage[offset + i] = bytes[i];
}

[[nodiscard]] cs2::CTraceFilter* grenadeConstructFilter(cs2::CTraceFilter* storage, void*, cs2::engine_trace::InteractionLayer interactsWith,
    cs2::CollisionGroup collisionGroup, cs2::PhysicsQueryFlag queryFlags) noexcept
{
    if (activeRecorder == nullptr)
        return nullptr;

    ++activeRecorder->filterConstructionCalls;
    if (activeRecorder->clearManagerOnFilterConstruction)
        *activeRecorder->physicsWorldPointerSlotStorage = nullptr;
    activeRecorder->constructedWithNativeArguments = interactsWith == engine_trace::grenade::kFirstInteraction
        && collisionGroup == engine_trace::grenade::kCollisionGroup && queryFlags == engine_trace::grenade::kQueryFlags;
    auto& filter = *storage;
    filter.writeValue(0x08, interactsWith);
    filter.writeValue(0x34, std::uint16_t{0xFFFF});
    filter.storage[0x36] = std::byte{};
    filter.storage[0x37] = std::byte{static_cast<std::uint8_t>(queryFlags)};
    filter.storage[0x38] = std::byte{static_cast<std::uint8_t>(collisionGroup)};
    filter.storage[activeRecorder->filterOverlayLayout.flagsOffset] = std::byte{0x49};
    filter.storage[activeRecorder->filterOverlayLayout.candidateCollectionModeOffset] = std::byte{0xA5};
    switch (activeRecorder->constructionOutcome) {
    case GrenadeConstructionOutcome::WrongReturn:
        return reinterpret_cast<cs2::CTraceFilter*>(filter.storage + 1);
    case GrenadeConstructionOutcome::Correct:
        return &filter;
    }
    return nullptr;
}

void grenadeAddSecondExcludedEntity(cs2::CTraceFilter* storage, void*, void*) noexcept
{
    if (activeRecorder == nullptr)
        return;

    ++activeRecorder->secondExclusionCalls;
    const auto& filter = *storage;
    activeRecorder->constructorFieldsPreservedBeforeSecondExclusion = filter.readValue<cs2::engine_trace::InteractionLayer>(0x08) == engine_trace::grenade::kFirstInteraction
        && filter.readValue<std::uint16_t>(0x34) == 0xFFFF
        && filter.storage[0x36] == std::byte{}
        && filter.storage[0x37] == std::byte{static_cast<std::uint8_t>(engine_trace::grenade::kQueryFlags)}
        && filter.storage[0x38] == std::byte{static_cast<std::uint8_t>(engine_trace::grenade::kCollisionGroup)};
    const auto& layout = activeRecorder->filterOverlayLayout;
    activeRecorder->overlayAppliedBeforeSecondExclusion = filter.readValue<cs2::engine_trace::InteractionLayer>(layout.interactsExcludeOffset) == engine_trace::grenade::kFilterInteractionMask
        && filter.readValue<cs2::engine_trace::InteractionLayer>(layout.interactsAsOffset) == engine_trace::grenade::kFilterObjectMask
        && filter.storage[layout.flagsOffset] == std::byte{0x4B}
        && filter.storage[layout.candidateCollectionModeOffset] == std::byte{0x01};
}

void grenadeBuildQueryShape(cs2::RnQueryShapeAttr_t*, const cs2::AABB_t* bounds) noexcept
{
    if (activeRecorder != nullptr) {
        ++activeRecorder->buildQueryShapeCalls;
        activeRecorder->bounds = *bounds;
    }
}

[[nodiscard]] bool grenadeTraceShape(cs2::PhysicsWorldPointerSlot, const cs2::RnQueryShapeAttr_t*, const cs2::Vector*, const cs2::Vector*, cs2::CTraceFilter*, cs2::CGameTrace* output) noexcept
{
    if (activeRecorder == nullptr)
        return false;

    ++activeRecorder->traceShapeCalls;
    activeRecorder->traceShapeCalledAfterSecondExclusion = activeRecorder->constructorFieldsPreservedBeforeSecondExclusion
        && activeRecorder->overlayAppliedBeforeSecondExclusion
        && activeRecorder->secondExclusionCalls == 1;
    auto& traceOutput = *output;
    writeOutput(traceOutput, 0x10, cs2::Vector{1.0f, 2.0f, 3.0f});
    writeOutput(traceOutput, 0x20, cs2::Vector{});
    writeOutput(traceOutput, 0x30, 1.0f);
    return true;
}

enum class GrenadeDependency {
    TraceShape,
    BuildQueryShape,
    ManagerStorage,
    FilterConstruction,
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

struct GrenadeEngineTraceContext {
    struct PatternSearchResults {
        template <typename T>
        [[nodiscard]] auto get() const noexcept
        {
            ++context.patternGetCalls;
            if constexpr (std::is_same_v<T, TraceShapeFunctionPointer>)
                return context.traceShapeAvailable ? &grenadeTraceShape : nullptr;
            else if constexpr (std::is_same_v<T, BuildRnQueryShapeAttrFromAABBFunctionPointer>)
                return context.buildQueryShapeAvailable ? &grenadeBuildQueryShape : nullptr;
            else if constexpr (std::is_same_v<T, PhysicsWorldPointerSlotStoragePointer>)
                return context.physicsWorldPointerSlotStorageAvailable ? &context.physicsWorldPointerSlot : nullptr;
            else if constexpr (std::is_same_v<T, CTraceFilterConstructionFunctionPointer>)
                return context.filterConstructionAvailable ? &grenadeConstructFilter : nullptr;
            else if constexpr (std::is_same_v<T, CTraceFilterAddExcludedEntityFunctionPointer>)
                return context.secondExclusionAvailable ? &grenadeAddSecondExcludedEntity : nullptr;
            else if constexpr (std::is_same_v<T, CGameTraceEndPositionOffset>)
                return CGameTraceOffset<cs2::Vector, std::int32_t>{context.endPositionOffset};
            else if constexpr (std::is_same_v<T, CGameTraceNormalOffset>)
                return CGameTraceOffset<cs2::Vector, std::int32_t>{context.normalOffset};
            else if constexpr (std::is_same_v<T, CGameTraceFractionOffset>)
                return CGameTraceOffset<float, std::int32_t>{context.fractionOffset};
            else if constexpr (std::is_same_v<T, CGameTraceRawEntityHandleOffset>)
                return CGameTraceOffset<std::int32_t, std::uint8_t>{context.rawEntityHandleOffset};
            else if constexpr (std::is_same_v<T, CTraceFilterInteractsExcludeOffset>)
                return CTraceFilterOffset<cs2::engine_trace::InteractionLayer>{context.filterOverlayLayout.interactsExcludeOffset};
            else if constexpr (std::is_same_v<T, CTraceFilterInteractsAsOffset>)
                return CTraceFilterOffset<cs2::engine_trace::InteractionLayer>{context.filterOverlayLayout.interactsAsOffset};
            else if constexpr (std::is_same_v<T, CTraceFilterFlagsOffset>)
                return CTraceFilterOffset<std::uint8_t>{context.filterOverlayLayout.flagsOffset};
            else {
                static_assert(std::is_same_v<T, CTraceFilterCandidateCollectionModeOffset>);
                return CTraceFilterOffset<std::uint8_t>{context.filterOverlayLayout.candidateCollectionModeOffset};
            }
        }

        GrenadeEngineTraceContext& context;
    };

    GrenadeEngineTraceContext() noexcept
        : results{*this}
    {
    }

    [[nodiscard]] const PatternSearchResults& patternSearchResults() const noexcept
    {
        ++patternSearchResultsCalls;
        return results;
    }
    void setDependencyAvailable(GrenadeDependency dependency, bool available) noexcept
    {
        switch (dependency) {
        case GrenadeDependency::TraceShape: traceShapeAvailable = available; break;
        case GrenadeDependency::BuildQueryShape: buildQueryShapeAvailable = available; break;
        case GrenadeDependency::ManagerStorage: physicsWorldPointerSlotStorageAvailable = available; break;
        case GrenadeDependency::FilterConstruction: filterConstructionAvailable = available; break;
        case GrenadeDependency::SecondExclusion: secondExclusionAvailable = available; break;
        case GrenadeDependency::EndPosition: endPositionOffset = available ? 0x10 : 0; break;
        case GrenadeDependency::Normal: normalOffset = available ? 0x20 : 0; break;
        case GrenadeDependency::Fraction: fractionOffset = available ? 0x30 : 0; break;
        case GrenadeDependency::RawEntityHandle: rawEntityHandleOffset = available ? 0x40 : 0; break;
        case GrenadeDependency::InteractsExclude: filterOverlayLayout.interactsExcludeOffset = available ? kCanonicalFilterOverlayLayout.interactsExcludeOffset : 0; break;
        case GrenadeDependency::InteractsAs: filterOverlayLayout.interactsAsOffset = available ? kCanonicalFilterOverlayLayout.interactsAsOffset : 0; break;
        case GrenadeDependency::Flags: filterOverlayLayout.flagsOffset = available ? kCanonicalFilterOverlayLayout.flagsOffset : 0; break;
        case GrenadeDependency::CandidateCollectionMode: filterOverlayLayout.candidateCollectionModeOffset = available ? kCanonicalFilterOverlayLayout.candidateCollectionModeOffset : 0; break;
        }
    }

    std::byte physicsWorldObject{};
    cs2::IVPhysics2World* physicsWorld{reinterpret_cast<cs2::IVPhysics2World*>(&physicsWorldObject)};
    cs2::PhysicsWorldPointerSlot physicsWorldPointerSlot{&physicsWorld};
    mutable int patternSearchResultsCalls{};
    mutable int patternGetCalls{};
    PatternSearchResults results;
    bool traceShapeAvailable{true};
    bool buildQueryShapeAvailable{true};
    bool physicsWorldPointerSlotStorageAvailable{true};
    bool filterConstructionAvailable{true};
    bool secondExclusionAvailable{true};
    std::int32_t endPositionOffset{0x10};
    std::int32_t normalOffset{0x20};
    std::int32_t fractionOffset{0x30};
    std::uint8_t rawEntityHandleOffset{0x40};
    engine_trace::grenade::FilterOverlayLayout filterOverlayLayout{kCanonicalFilterOverlayLayout};
};

using GrenadeEngineTrace = EngineTrace<GrenadeEngineTraceContext>;

TEST(EngineTraceGrenadeTest, RequiresEveryOperationalBinding)
{
    constexpr std::array dependencies{
        GrenadeDependency::TraceShape,
        GrenadeDependency::BuildQueryShape,
        GrenadeDependency::ManagerStorage,
        GrenadeDependency::FilterConstruction,
        GrenadeDependency::SecondExclusion,
        GrenadeDependency::EndPosition,
        GrenadeDependency::Normal,
        GrenadeDependency::Fraction,
        GrenadeDependency::RawEntityHandle,
        GrenadeDependency::InteractsExclude,
        GrenadeDependency::InteractsAs,
        GrenadeDependency::Flags,
        GrenadeDependency::CandidateCollectionMode
    };

    for (const auto dependency : dependencies) {
        GrenadeEngineTraceContext context;
        GrenadeTraceRecorder recorder;
        ActiveRecorderGuard activeRecorderGuard{recorder};
        context.setDependencyAvailable(dependency, false);
        GrenadeEngineTrace trace{context};

        EXPECT_FALSE(trace.isGrenadeHullTraceAvailable());
        EXPECT_FALSE(trace.traceGrenadeHull(grenadeRequest({}, {})).hasValue());
        EXPECT_EQ(recorder.filterConstructionCalls, 0);
        EXPECT_EQ(recorder.secondExclusionCalls, 0);
        EXPECT_EQ(recorder.buildQueryShapeCalls, 0);
        EXPECT_EQ(recorder.traceShapeCalls, 0);
    }
}

TEST(EngineTraceGrenadeTest, RejectsNonFiniteInFlightInputsBeforeNativeCalls)
{
    constexpr auto nan = std::bit_cast<float>(std::uint32_t{0x7FC00000});
    constexpr auto infinity = std::bit_cast<float>(std::uint32_t{0x7F800000});
    constexpr std::array invalidVectors{
        cs2::Vector{nan, 0.0f, 0.0f},
        cs2::Vector{infinity, 0.0f, 0.0f}
    };

    GrenadeEngineTraceContext context;
    GrenadeTraceRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    GrenadeEngineTrace trace{context};

    for (const auto invalid : invalidVectors) {
        EXPECT_FALSE(trace.traceGrenadeHull(grenadeRequest(invalid, {})).hasValue());
        EXPECT_FALSE(trace.traceGrenadeHull(grenadeRequest({}, invalid)).hasValue());
    }

    EXPECT_EQ(recorder.filterConstructionCalls, 0);
    EXPECT_EQ(recorder.secondExclusionCalls, 0);
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceGrenadeTest, RequiresValidOutputAndManagerDependencies)
{
    GrenadeEngineTraceContext context;
    GrenadeTraceRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    GrenadeEngineTrace trace{context};

    context.endPositionOffset = 0;
    EXPECT_FALSE(trace.isGrenadeHullTraceAvailable());
    context.endPositionOffset = 0x10;
    context.normalOffset = 0;
    EXPECT_FALSE(trace.isGrenadeHullTraceAvailable());
    context.normalOffset = 0x20;
    context.fractionOffset = 0;
    EXPECT_FALSE(trace.isGrenadeHullTraceAvailable());
    context.fractionOffset = 0x30;
    context.rawEntityHandleOffset = 0;
    EXPECT_FALSE(trace.isGrenadeHullTraceAvailable());
    context.rawEntityHandleOffset = 0x40;
    context.physicsWorldPointerSlot = nullptr;
    EXPECT_FALSE(trace.isGrenadeHullTraceAvailable());
    EXPECT_FALSE(trace.traceGrenadeHull(grenadeRequest({}, {})).hasValue());
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceGrenadeTest, InvalidFilterLayoutsAreUnavailableBeforeNativeCalls)
{
    constexpr std::array invalidLayouts{
        engine_trace::grenade::FilterOverlayLayout{
            .interactsExcludeOffset = 0,
            .interactsAsOffset = 0x18,
            .flagsOffset = 0x39,
            .candidateCollectionModeOffset = 0x40
        },
        engine_trace::grenade::FilterOverlayLayout{
            .interactsExcludeOffset = 0x48,
            .interactsAsOffset = 0x18,
            .flagsOffset = 0x39,
            .candidateCollectionModeOffset = 0x40
        },
        engine_trace::grenade::FilterOverlayLayout{
            .interactsExcludeOffset = 0x11,
            .interactsAsOffset = 0x18,
            .flagsOffset = 0x39,
            .candidateCollectionModeOffset = 0x40
        },
        engine_trace::grenade::FilterOverlayLayout{
            .interactsExcludeOffset = 0x10,
            .interactsAsOffset = 0x18,
            .flagsOffset = 0x18,
            .candidateCollectionModeOffset = 0x40
        }
    };

    for (const auto layout : invalidLayouts) {
        GrenadeEngineTraceContext context;
        GrenadeTraceRecorder recorder;
        ActiveRecorderGuard activeRecorderGuard{recorder};
        context.filterOverlayLayout = layout;
        GrenadeEngineTrace trace{context};

        EXPECT_FALSE(trace.isGrenadeHullTraceAvailable());
        EXPECT_FALSE(trace.traceGrenadeHull(grenadeRequest({}, {})).hasValue());
        EXPECT_EQ(recorder.filterConstructionCalls, 0);
        EXPECT_EQ(recorder.secondExclusionCalls, 0);
        EXPECT_EQ(recorder.traceShapeCalls, 0);
    }
}

TEST(EngineTraceGrenadeTest, RejectsWrongFilterConstructionReturnBeforeTracing)
{
    GrenadeEngineTraceContext context;
    GrenadeTraceRecorder recorder{.constructionOutcome = GrenadeConstructionOutcome::WrongReturn};
    ActiveRecorderGuard activeRecorderGuard{recorder};
    GrenadeEngineTrace trace{context};

    EXPECT_TRUE(trace.isGrenadeHullTraceAvailable());
    EXPECT_FALSE(trace.traceGrenadeHull(grenadeRequest({}, {})).hasValue());
    EXPECT_EQ(recorder.filterConstructionCalls, 1);
    EXPECT_EQ(recorder.secondExclusionCalls, 0);
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST(EngineTraceGrenadeTest, AppliesOverlayThenSecondExclusionBeforeTracing)
{
    GrenadeEngineTraceContext context;
    GrenadeTraceRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    GrenadeEngineTrace trace{context};
    std::byte firstExcluded{};
    std::byte secondExcluded{};

    const auto result = trace.traceGrenadeHull(grenadeRequest({}, {}, {&firstExcluded, &secondExcluded}));

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(recorder.filterConstructionCalls, 1);
    EXPECT_TRUE(recorder.constructedWithNativeArguments);
    EXPECT_EQ(recorder.secondExclusionCalls, 1);
    EXPECT_EQ(recorder.buildQueryShapeCalls, 1);
    EXPECT_EQ(recorder.bounds.m_vMinBounds, (cs2::Vector{-2.0f, -2.0f, -2.0f}));
    EXPECT_EQ(recorder.bounds.m_vMaxBounds, (cs2::Vector{2.0f, 2.0f, 2.0f}));
    EXPECT_EQ(recorder.traceShapeCalls, 1);
    EXPECT_TRUE(recorder.constructorFieldsPreservedBeforeSecondExclusion);
    EXPECT_TRUE(recorder.overlayAppliedBeforeSecondExclusion);
    EXPECT_TRUE(recorder.traceShapeCalledAfterSecondExclusion);
}

TEST(EngineTraceGrenadeTest, UsesOneBindingSnapshotForTheWholeTraceCall)
{
    GrenadeEngineTraceContext context;
    GrenadeTraceRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    GrenadeEngineTrace trace{context};

    const auto result = trace.traceGrenadeHull(grenadeRequest({}, {}));

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(context.patternSearchResultsCalls, 1);
    EXPECT_EQ(context.patternGetCalls, 13);
    EXPECT_EQ(recorder.filterConstructionCalls, 1);
    EXPECT_EQ(recorder.secondExclusionCalls, 0);
    EXPECT_EQ(recorder.traceShapeCalls, 1);
}

TEST(EngineTraceGrenadeTest, FailsClosedWhenManagerBecomesNullBeforeTraceInvocation)
{
    GrenadeEngineTraceContext context;
    GrenadeTraceRecorder recorder{
        .physicsWorldPointerSlotStorage = &context.physicsWorldPointerSlot,
        .clearManagerOnFilterConstruction = true
    };
    ActiveRecorderGuard activeRecorderGuard{recorder};
    GrenadeEngineTrace trace{context};

    EXPECT_FALSE(trace.traceGrenadeHull(grenadeRequest({}, {})).hasValue());
    EXPECT_EQ(recorder.filterConstructionCalls, 1);
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

}
