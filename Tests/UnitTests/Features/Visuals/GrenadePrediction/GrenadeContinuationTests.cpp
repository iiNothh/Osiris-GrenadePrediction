#include <limits>

#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadeSimulator.h>
#include <GameClient/EngineTrace/EngineTraceTypes.h>
#include <Mocks/GrenadePrediction/ScriptedGrenadeTrace.h>

namespace
{

using Simulator = GrenadeSimulator<GrenadeSimulatorTestHookContext>;

[[nodiscard]] GrenadePlayerCollisionSnapshot availablePlayerCollisionSnapshot() noexcept
{
    GrenadePlayerCollisionSnapshot snapshot;
    snapshot.status = GrenadePlayerCollisionSnapshotStatus::Available;
    return snapshot;
}

[[nodiscard]] GrenadeContinuationInput continuationInput(cs2::Vector origin, cs2::Vector velocity, float elapsedTime,
    int consumedWorldContacts = 0, bool playerResponseConsumed = false) noexcept
{
    return {{origin, velocity}, elapsedTime, consumedWorldContacts, playerResponseConsumed};
}

TEST(GrenadeContinuationTest, StartsAtTheSuppliedOriginAndUsesRemainingFuseTime)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};
    Trajectory trajectory;
    const auto input = continuationInput({100.0f, 20.0f, 30.0f}, {100.0f, 0.0f, 0.0f}, 1.625f);

    simulator.simulateContinuation(trajectory, input, GrenadeKind::HEGrenade, nullptr, 800.0f);

    ASSERT_TRUE(trajectory.valid);
    ASSERT_GE(trajectory.pointsCount, 1);
    EXPECT_EQ(trajectory.points[0], input.launch.origin);
    EXPECT_FLOAT_EQ(trajectory.elapsedTimes[0], input.elapsedTime);
    EXPECT_LT(trajectory.pointsCount, 4);
}

TEST(GrenadeContinuationTest, CarriesTheConsumedWorldContactBudget)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.push(TraceResult{0.5f, {}, {-1.0f, 0.0f, 0.0f}, engine_trace::kWorldEntityHandle, true});
    context.trace.clearAfterScript();
    Simulator simulator{context};
    Trajectory trajectory;
    const auto input = continuationInput({}, {1000.0f, 0.0f, 0.0f}, 0.5f, grenade_prediction_params::kMaxBounces);

    simulator.simulateContinuation(trajectory, input, GrenadeKind::SmokeGrenade, nullptr, 800.0f);

    EXPECT_TRUE(trajectory.valid);
    EXPECT_EQ(trajectory.worldContactMarkersCount, 1);
}

TEST(GrenadeContinuationTest, DoesNotRepeatAConsumedPlayerResponseOrChangeFirstExclusion)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f}, true};
    simulator.setPlayerCollisionSnapshot(&snapshot);
    Trajectory trajectory;
    void* const firstExclusion = &context;
    const auto input = continuationInput({1.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, 1.0f, 0, true);

    simulator.simulateContinuation(trajectory, input, GrenadeKind::HEGrenade, firstExclusion, 800.0f);

    ASSERT_TRUE(trajectory.valid);
    EXPECT_EQ(trajectory.markersCount, 0);
    EXPECT_EQ(context.trace.lastExcludedFirst, firstExclusion);
}

TEST(GrenadeContinuationTest, ReusesTheStartPointForAPlayerResponseAtTheSameElapsedTime)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f}, true};
    simulator.setPlayerCollisionSnapshot(&snapshot);
    Trajectory trajectory;
    const auto input = continuationInput({1.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, 1.62f);

    simulator.simulateContinuation(trajectory, input, GrenadeKind::HEGrenade, nullptr, 800.0f);

    ASSERT_TRUE(trajectory.valid);
    ASSERT_EQ(trajectory.markersCount, 1);
    EXPECT_EQ(trajectory.markers[0].kind, TrajectoryMarkerKind::PlayerResponse);
    EXPECT_EQ(trajectory.markers[0].pointIndex, 0);
    EXPECT_FLOAT_EQ(trajectory.elapsedTimes[0], input.elapsedTime);
}

TEST(GrenadeContinuationTest, EmitsStrictlyMonotonicPhysicalTimesForContactPoints)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.push(TraceResult{0.5f, {}, {-1.0f, 0.0f, 0.0f}, engine_trace::kWorldEntityHandle, true});
    context.trace.clearAfterScript();
    Simulator simulator{context};
    Trajectory trajectory;
    const auto input = continuationInput({}, {1000.0f, 0.0f, 0.0f}, 1.62f);

    simulator.simulateContinuation(trajectory, input, GrenadeKind::HEGrenade, nullptr, 800.0f);

    ASSERT_TRUE(trajectory.valid);
    ASSERT_EQ(trajectory.worldContactMarkersCount, 1);
    for (int i = 1; i < trajectory.pointsCount; ++i)
        EXPECT_LT(trajectory.elapsedTimes[i - 1], trajectory.elapsedTimes[i]);
    const auto contactPointIndex = trajectory.markers[0].pointIndex;
    EXPECT_NEAR(trajectory.elapsedTimes[contactPointIndex], input.elapsedTime + grenade_prediction_params::kMovementSubstepDt * 0.5f, 0.000001f);
}

TEST(GrenadeContinuationTest, InvalidInputLeavesOutputInvalid)
{
    GrenadeSimulatorTestHookContext context;
    Simulator simulator{context};
    Trajectory trajectory;
    ASSERT_TRUE(trajectory.appendPoint({}, 0.0f));
    trajectory.valid = true;
    const auto input = continuationInput({}, {100.0f, 0.0f, 0.0f}, std::numeric_limits<float>::quiet_NaN());

    simulator.simulateContinuation(trajectory, input, GrenadeKind::HEGrenade, nullptr, 800.0f);

    EXPECT_FALSE(trajectory.valid);
    EXPECT_EQ(trajectory.pointsCount, 0);
}

}
