#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include <CS2/Constants/CollisionGroup.h>
#include <CS2/Constants/InteractionLayers.h>
#include <CS2/Constants/PhysicsQueryFlag.h>
#include <Features/Visuals/GrenadePrediction/GrenadeTracePreset.h>
#include <GameClient/EngineTrace/HullTraceRequest.h>
#include <GameClient/EngineTrace/TraceResult.h>

namespace
{

struct RecordingTrace {
    engine_trace::HullTraceRequest genericRequest{};
    engine_trace::HullTraceRequest grenadeRequest{};
    bool genericCalled{};
    bool grenadeCalled{};

    [[nodiscard]] Optional<TraceResult> traceHull(const engine_trace::HullTraceRequest& request) noexcept
    {
        genericRequest = request;
        genericCalled = true;
        return TraceResult{};
    }

    [[nodiscard]] bool isGrenadeHullTraceAvailable() const noexcept { return true; }

    [[nodiscard]] Optional<TraceResult> traceGrenadeHull(const engine_trace::HullTraceRequest& request) noexcept
    {
        grenadeRequest = request;
        grenadeCalled = true;
        return TraceResult{};
    }
};

void expectInFlightGrenadeRequest(const engine_trace::HullTraceRequest& request, cs2::Vector start, cs2::Vector end,
    void* firstExcluded, void* secondExcluded) noexcept
{
    EXPECT_EQ(request.start, start);
    EXPECT_EQ(request.end, end);
    EXPECT_EQ(request.mins, (cs2::Vector{-2.0f, -2.0f, -2.0f}));
    EXPECT_EQ(request.maxs, (cs2::Vector{2.0f, 2.0f, 2.0f}));
    EXPECT_EQ(request.excludedEntities.first, firstExcluded);
    EXPECT_EQ(request.excludedEntities.second, secondExcluded);
    EXPECT_EQ(request.filter.interactsWith, engine_trace::grenade::kFirstInteraction);
    EXPECT_EQ(request.filter.collisionGroup, engine_trace::grenade::kCollisionGroup);
    EXPECT_EQ(request.filter.queryFlags, engine_trace::grenade::kQueryFlags);
}

void expectSpawnGrenadeRequest(const engine_trace::HullTraceRequest& request, cs2::Vector start, cs2::Vector end, void* excluded) noexcept
{
    EXPECT_EQ(request.start, start);
    EXPECT_EQ(request.end, end);
    EXPECT_EQ(request.mins, (cs2::Vector{-2.0f, -2.0f, -2.0f}));
    EXPECT_EQ(request.maxs, (cs2::Vector{2.0f, 2.0f, 2.0f}));
    EXPECT_EQ(request.excludedEntities.first, excluded);
    EXPECT_EQ(request.excludedEntities.second, nullptr);
    EXPECT_EQ(static_cast<std::uint64_t>(request.filter.interactsWith), 0x001C200Bull);
    EXPECT_EQ(request.filter.collisionGroup, cs2::CollisionGroup::Default);
    EXPECT_EQ(request.filter.queryFlags, cs2::PhysicsQueryFlag::IncludeSolidContacts
        | cs2::PhysicsQueryFlag::RespectDisabledSolidContacts | cs2::PhysicsQueryFlag::IncludeTriggerContacts);
}

TEST(GrenadeTracePresetTest, ProducesTheExactInFlightRequest)
{
    std::byte firstExcluded{};
    std::byte secondExcluded{};
    constexpr cs2::Vector start{1.0f, 2.0f, 3.0f};
    constexpr cs2::Vector end{4.0f, 5.0f, 6.0f};

    const auto request = grenade_trace_preset::makeRequest(start, end, {&firstExcluded, &secondExcluded});

    expectInFlightGrenadeRequest(request, start, end, &firstExcluded, &secondExcluded);
}

TEST(GrenadeTracePresetTest, PreservesCustomFilterAndBothExclusionsInRequest)
{
    std::byte firstExcluded{};
    std::byte secondExcluded{};
    constexpr engine_trace::TraceFilterParameters filter{
        .interactsWith = cs2::engine_trace::InteractionLayer::Hitboxes | cs2::engine_trace::InteractionLayer::Player,
        .collisionGroup = cs2::CollisionGroup::Default,
        .queryFlags = cs2::PhysicsQueryFlag::IncludeTriggerContacts
    };

    const auto request = grenade_trace_preset::makeRequest({}, {}, {&firstExcluded, &secondExcluded}, filter);

    EXPECT_EQ(request.excludedEntities.first, &firstExcluded);
    EXPECT_EQ(request.excludedEntities.second, &secondExcluded);
    EXPECT_EQ(request.filter.interactsWith, filter.interactsWith);
    EXPECT_EQ(request.filter.collisionGroup, filter.collisionGroup);
    EXPECT_EQ(request.filter.queryFlags, filter.queryFlags);
}

TEST(GrenadeTracePresetTest, SelectsTheGenericAndGrenadeFacadeOperations)
{
    RecordingTrace trace;
    std::byte firstExcluded{};
    std::byte secondExcluded{};
    constexpr cs2::Vector start{1.0f, 2.0f, 3.0f};
    constexpr cs2::Vector end{4.0f, 5.0f, 6.0f};

    ASSERT_TRUE(grenade_trace_preset::traceSpawnHull(trace, start, end, &firstExcluded).hasValue());
    ASSERT_TRUE(grenade_trace_preset::traceInFlightHull(trace, start, end, {&firstExcluded, &secondExcluded}).hasValue());

    EXPECT_TRUE(trace.genericCalled);
    EXPECT_TRUE(trace.grenadeCalled);
    expectSpawnGrenadeRequest(trace.genericRequest, start, end, &firstExcluded);
    expectInFlightGrenadeRequest(trace.grenadeRequest, start, end, &firstExcluded, &secondExcluded);
}

TEST(GrenadeTracePresetTest, PreservesSecondExclusionSlotForPaneContinuation)
{
    RecordingTrace trace;
    std::byte pane{};

    ASSERT_TRUE(grenade_trace_preset::traceInFlightHull(trace, {}, {}, {nullptr, &pane}).hasValue());

    EXPECT_TRUE(trace.grenadeCalled);
    expectInFlightGrenadeRequest(trace.grenadeRequest, {}, {}, nullptr, &pane);
}

TEST(GrenadeTracePresetTest, ForwardsCustomFilterForInFlightTrace)
{
    RecordingTrace trace;
    std::byte firstExcluded{};
    std::byte secondExcluded{};
    constexpr engine_trace::TraceFilterParameters filter{
        .interactsWith = cs2::engine_trace::InteractionLayer::Hitboxes,
        .collisionGroup = cs2::CollisionGroup::Default,
        .queryFlags = cs2::PhysicsQueryFlag::RespectIgnoredPairs
    };

    ASSERT_TRUE(grenade_trace_preset::traceInFlightHull(trace, {}, {}, {&firstExcluded, &secondExcluded}, filter).hasValue());

    EXPECT_EQ(trace.grenadeRequest.excludedEntities.first, &firstExcluded);
    EXPECT_EQ(trace.grenadeRequest.excludedEntities.second, &secondExcluded);
    EXPECT_EQ(trace.grenadeRequest.filter.interactsWith, filter.interactsWith);
    EXPECT_EQ(trace.grenadeRequest.filter.collisionGroup, filter.collisionGroup);
    EXPECT_EQ(trace.grenadeRequest.filter.queryFlags, filter.queryFlags);
}

}
