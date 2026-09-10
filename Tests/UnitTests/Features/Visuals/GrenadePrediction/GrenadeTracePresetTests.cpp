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

void expectLegacyGrenadeRequest(const engine_trace::HullTraceRequest& request, cs2::Vector start, cs2::Vector end,
    void* firstExcluded, void* secondExcluded) noexcept
{
    EXPECT_EQ(request.start, start);
    EXPECT_EQ(request.end, end);
    EXPECT_EQ(request.mins, (cs2::Vector{-2.0f, -2.0f, -2.0f}));
    EXPECT_EQ(request.maxs, (cs2::Vector{2.0f, 2.0f, 2.0f}));
    EXPECT_EQ(request.excludedEntities.first, firstExcluded);
    EXPECT_EQ(request.excludedEntities.second, secondExcluded);
    EXPECT_EQ(request.filter.interactsWith, grenade_trace_preset::kGrenadeInteractionLayerMask);
    EXPECT_EQ(request.filter.collisionGroup, cs2::CollisionGroup::Default);
    EXPECT_EQ(request.filter.queryFlags, cs2::PhysicsQueryFlag::IncludeSolidContacts
        | cs2::PhysicsQueryFlag::RespectDisabledSolidContacts | cs2::PhysicsQueryFlag::IncludeTriggerContacts);
}

TEST(GrenadeTracePresetTest, ProducesTheExactLegacyRequest)
{
    std::byte firstExcluded{};
    std::byte secondExcluded{};
    constexpr cs2::Vector start{1.0f, 2.0f, 3.0f};
    constexpr cs2::Vector end{4.0f, 5.0f, 6.0f};

    const auto request = grenade_trace_preset::makeRequest(start, end, {&firstExcluded, &secondExcluded});

    EXPECT_EQ(static_cast<std::uint64_t>(grenade_trace_preset::kGrenadeInteractionLayerMask), 0x001C200Bull);
    expectLegacyGrenadeRequest(request, start, end, &firstExcluded, &secondExcluded);
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
    expectLegacyGrenadeRequest(trace.genericRequest, start, end, &firstExcluded, nullptr);
    expectLegacyGrenadeRequest(trace.grenadeRequest, start, end, &firstExcluded, &secondExcluded);
}

TEST(GrenadeTracePresetTest, PreservesSecondExclusionSlotForPaneContinuation)
{
    RecordingTrace trace;
    std::byte pane{};

    ASSERT_TRUE(grenade_trace_preset::traceInFlightHull(trace, {}, {}, {nullptr, &pane}).hasValue());

    EXPECT_TRUE(trace.grenadeCalled);
    expectLegacyGrenadeRequest(trace.grenadeRequest, {}, {}, nullptr, &pane);
}

}
