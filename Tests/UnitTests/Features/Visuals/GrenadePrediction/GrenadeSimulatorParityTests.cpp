#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadeGravity.h>
#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionMirror.h>
#include <GameClient/Entities/GrenadeKind.h>
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

TEST(GrenadeSimulationParityTest, FallsBackToDefaultGravity)
{
    ScriptedGrenadeCvarSystem cvars;
    EXPECT_FLOAT_EQ(grenade_prediction::resolveServerGravity(cvars), 800.0f);
    cvars.gravity = std::numeric_limits<float>::infinity();
    EXPECT_FLOAT_EQ(grenade_prediction::resolveServerGravity(cvars), 800.0f);
    cvars.gravity = -1.0f;
    EXPECT_FLOAT_EQ(grenade_prediction::resolveServerGravity(cvars), 800.0f);
}

TEST(GrenadeSimulationParityTest, UsesReferenceTrajectoryCapacityAndExhaustionPolicy)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};
    Trajectory trajectory;
    simulator.simulate(trajectory, {{}, {100.0f, 0.0f, 0.0f}}, GrenadeKind::SmokeGrenade, nullptr, 800.0f);
    ASSERT_TRUE(trajectory.valid);
    EXPECT_EQ(Trajectory::kPointsCapacity, 500);
    EXPECT_EQ(trajectory.pointsCount, Trajectory::kPointsCapacity);
    EXPECT_NEAR(trajectory.endPos.x, 1803.125f, 0.001f);
}

TEST(GrenadeSimulationParityTest, RecordsContactsUntilReferenceMarkerCapacity)
{
    GrenadeSimulatorTestHookContext context;
    for (int i{}; i <= 20; ++i) {
        context.trace.push(TraceResult{0.5f, {}, {-1.0f, 0.0f, 0.0f}, engine_trace::kWorldEntityHandle, true});
        context.trace.push(TraceResult{1.0f, {}, {}});
    }
    context.trace.clearAfterScript();
    Simulator simulator{context};
    Trajectory trajectory;
    simulator.simulate(trajectory, {{}, {1.0e12f, 0.0f, 0.0f}}, GrenadeKind::SmokeGrenade, nullptr, 800.0f);
    EXPECT_TRUE(trajectory.valid);
    EXPECT_EQ(trajectory.worldContactMarkersCount, 20);
    EXPECT_EQ(trajectory.markersCount, 20);
}

TEST(GrenadeSimulationParityTest, StoresReferenceFreeFlightKinematics)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};
    Trajectory trajectory;
    simulator.simulate(trajectory, {{}, {100.0f, 0.0f, 0.0f}}, GrenadeKind::HEGrenade, nullptr, 800.0f);
    ASSERT_TRUE(trajectory.valid);
    ASSERT_EQ(trajectory.pointsCount, 54);
    EXPECT_NEAR(trajectory.points[1].x, 3.125f, 0.001f);
    EXPECT_NEAR(trajectory.points[1].z, -0.15625f, 0.001f);
    EXPECT_NEAR(trajectory.endPos.z, -430.6640625f, 0.001f);
}

TEST(GrenadeSimulationParityTest, UsesReferenceDecoyAndSmokeTimeoutTicks)
{
    EXPECT_FALSE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::shouldDetonate(GrenadeKind::Decoy, 640));
    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::shouldDetonate(GrenadeKind::Decoy, 641));
    EXPECT_FALSE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::shouldDetonate(GrenadeKind::SmokeGrenade, 1152));
    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::shouldDetonate(GrenadeKind::SmokeGrenade, 1153));
}

TEST(GrenadeSimulationParityTest, ContinuesTraceForTheUnusedCollisionSubstep)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.push(TraceResult{0.5f, {}, {-1.0f, 0.0f, 0.0f}, engine_trace::kWorldEntityHandle, true});
    context.trace.clearAfterScript();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{1000.0f, 0.0f, 0.0f};
    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade).traceSucceeded);
    EXPECT_EQ(context.trace.calls, 3);
}

TEST(GrenadeSimulationParityTest, DampsDynamicPaneAndExcludesItFromLaterTraces)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::CEntityHandle pane{42u};
    context.entitySystem.setDynamicProp(pane);
    context.trace.push(TraceResult{0.5f, {4.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, static_cast<std::int32_t>(pane.value), true});
    context.trace.clearAfterScript();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 5.0f, 5.0f};
    void* const owner = &context;
    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade, owner).traceSucceeded);
    EXPECT_EQ(velocity, (cs2::Vector{40.0f, 2.0f, -1.5f}));
    EXPECT_EQ(position, (cs2::Vector{4.46875f, 0.0234375f, 0.00390625f}));
    EXPECT_EQ(context.trace.calls, 3);
    EXPECT_EQ(context.trace.lastExcludedFirst, owner);
    EXPECT_EQ(context.trace.lastExcludedSecond, &context.entitySystem.entity);
}

TEST(GrenadeSimulationParityTest, UsesNativeProfileInFlightTraceWithoutGenericFallback)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade).traceSucceeded);
    EXPECT_EQ(context.trace.genericCalls, 0);
    EXPECT_EQ(context.trace.inFlightCalls, 2);
    EXPECT_EQ(context.trace.lastExcludedFirst, nullptr);
    EXPECT_EQ(context.trace.lastExcludedSecond, nullptr);
}

TEST(GrenadeSimulationParityTest, FailsWhenTheNativeProfileIsUnavailableWithoutUsingGenericTrace)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.inFlightTraceAvailable = false;
    context.trace.clearAfterScript();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    EXPECT_FALSE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade).traceSucceeded);
    EXPECT_EQ(context.trace.genericCalls, 0);
    EXPECT_EQ(context.trace.inFlightCalls, 1);
    EXPECT_EQ(context.trace.calls, 0);
}

TEST(GrenadeSimulationParityTest, RejectsAnInFlightHitWithoutARawEntityHandle)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.push(TraceResult{0.5f, {}, {-1.0f, 0.0f, 0.0f}});
    Simulator simulator{context};
    Trajectory trajectory;

    simulator.simulate(trajectory, {{}, {100.0f, 0.0f, 0.0f}}, GrenadeKind::HEGrenade, nullptr, 800.0f);

    EXPECT_FALSE(trajectory.valid);
    EXPECT_EQ(trajectory.endPos, cs2::Vector{});
}

TEST(GrenadeSimulationParityTest, RejectsTheSamePaneReturnedAfterItWasExcluded)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::CEntityHandle pane{42u};
    context.entitySystem.setDynamicProp(pane);
    context.trace.push(TraceResult{0.5f, {4.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, static_cast<std::int32_t>(pane.value), true});
    context.trace.push(TraceResult{0.5f, {4.1f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, static_cast<std::int32_t>(pane.value), true});
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    EXPECT_FALSE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade).traceSucceeded);
}

TEST(GrenadeSimulationParityTest, AppliesAnOrdinaryResponseWhenPaneContinuationReachesWorld)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::CEntityHandle pane{42u};
    context.entitySystem.setDynamicProp(pane);
    context.trace.push(TraceResult{0.5f, {4.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, static_cast<std::int32_t>(pane.value), true});
    context.trace.push(TraceResult{0.5f, {4.1f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, engine_trace::kWorldEntityHandle, true});
    context.trace.clearAfterScript();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{200.0f, 0.0f, 0.0f};

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, GrenadeKind::HEGrenade);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_EQ(result.contactsCount, 1);
    EXPECT_EQ(context.trace.calls, 2);
    EXPECT_EQ(context.trace.lastStart, (cs2::Vector{4.0f, 0.0f, 0.0f}));
    EXPECT_EQ(context.trace.lastEnd, (cs2::Vector{4.3125f, 0.0f, -0.001953125f}));
    EXPECT_EQ(context.trace.lastExcludedSecond, &context.entitySystem.entity);
    EXPECT_NEAR(velocity.x, -36.0140625f, 0.001f);
    EXPECT_NEAR(velocity.z, -0.45f, 0.001f);
}

TEST(GrenadeSimulationParityTest, AppliesAnOrdinaryResponseToAResolvedNonDynamicEntity)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::CEntityHandle entity{43u};
    context.entitySystem.setNonDynamicProp(entity);
    context.trace.push(TraceResult{0.5f, {4.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, static_cast<std::int32_t>(entity.value), true});
    context.trace.clearAfterScript();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_EQ(result.contactsCount, 1);
    EXPECT_LT(velocity.x, 0.0f);
}

TEST(GrenadeSimulationParityTest, AppliesSteepFloorDampingOnlyWhenWorldHandleIsRead)
{
    GrenadeSimulatorTestHookContext context;
    Simulator simulator{context};
    cs2::Vector worldVelocity{100.0f, 0.0f, -1000.0f};
    cs2::Vector unknownVelocity = worldVelocity;
    static_cast<void>(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::applyContactResponse(
        simulator, {0.5f, {}, {0.0f, 0.0f, 1.0f}, engine_trace::kWorldEntityHandle, true}, worldVelocity, GrenadeKind::HEGrenade));
    static_cast<void>(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::applyContactResponse(
        simulator, {0.5f, {}, {0.0f, 0.0f, 1.0f}, engine_trace::kWorldEntityHandle, false}, unknownVelocity, GrenadeKind::HEGrenade));
    EXPECT_NEAR(worldVelocity.z, 227.24025f, 0.001f);
    EXPECT_NEAR(unknownVelocity.z, 450.0140625f, 0.001f);
}

TEST(GrenadePlayerCollisionSelectionTest, DeeperOverlapBeatsLowerHandle)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {2.0f, -1.0f, -1.0f}, {4.0f, 1.0f, 1.0f}, true};
    snapshot.candidates[snapshot.count++] = {100, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};

    const auto* const selected = grenade_player_collision_mirror::select(snapshot, {});

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->rawHandle, 100);
}

TEST(GrenadePlayerCollisionSelectionTest, NestedContainingBoundsUseNearestCenter)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {-10.0f, -10.0f, -10.0f}, {10.0f, 10.0f, 10.0f}, true};
    snapshot.candidates[snapshot.count++] = {100, {0.0f, -1.0f, -1.0f}, {2.0f, 1.0f, 1.0f}, true};

    const auto* const selected = grenade_player_collision_mirror::select(snapshot, {1.0f, 0.0f, 0.0f});

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->rawHandle, 100);
}

TEST(GrenadePlayerCollisionSelectionTest, RanksActualCenterDistanceInsteadOfLargestAxisDistance)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {-4.0f, -4.0f, -4.0f}, {0.0f, 0.0f, 0.0f}, true};
    snapshot.candidates[snapshot.count++] = {100, {-4.4f, -1.0f, -1.0f}, {0.0f, 1.0f, 1.0f}, true};

    const auto* const selected = grenade_player_collision_mirror::select(snapshot, {});

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->rawHandle, 100);
}

TEST(GrenadePlayerCollisionSelectionTest, ReversingSnapshotOrderDoesNotChangeSelection)
{
    auto firstSnapshot = availablePlayerCollisionSnapshot();
    firstSnapshot.candidates[firstSnapshot.count++] = {1, {2.0f, -1.0f, -1.0f}, {4.0f, 1.0f, 1.0f}, true};
    firstSnapshot.candidates[firstSnapshot.count++] = {100, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};
    auto secondSnapshot = availablePlayerCollisionSnapshot();
    secondSnapshot.candidates[secondSnapshot.count++] = firstSnapshot.candidates[1];
    secondSnapshot.candidates[secondSnapshot.count++] = firstSnapshot.candidates[0];

    const auto* const firstSelection = grenade_player_collision_mirror::select(firstSnapshot, {});
    const auto* const secondSelection = grenade_player_collision_mirror::select(secondSnapshot, {});

    ASSERT_NE(firstSelection, nullptr);
    ASSERT_NE(secondSelection, nullptr);
    EXPECT_EQ(firstSelection->rawHandle, 100);
    EXPECT_EQ(secondSelection->rawHandle, firstSelection->rawHandle);
}

TEST(GrenadePlayerCollisionSelectionTest, IdenticalGeometryUsesLowestHandle)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {100, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};
    snapshot.candidates[snapshot.count++] = {1, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};

    const auto* const selected = grenade_player_collision_mirror::select(snapshot, {});

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->rawHandle, 1);
}

TEST(GrenadePlayerCollisionSelectionTest, SkipsIneligibleCandidate)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, false};
    snapshot.candidates[snapshot.count++] = {100, {-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f}, true};

    const auto* const selected = grenade_player_collision_mirror::select(snapshot, {});

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->rawHandle, 100);
}

TEST(GrenadePlayerCollisionSelectionTest, FailsClosedForUnavailableOrInvalidInput)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};

    snapshot.status = GrenadePlayerCollisionSnapshotStatus::Unavailable;
    EXPECT_EQ(grenade_player_collision_mirror::select(snapshot, {}), nullptr);
    snapshot.status = GrenadePlayerCollisionSnapshotStatus::Available;
    snapshot.count = -1;
    EXPECT_EQ(grenade_player_collision_mirror::select(snapshot, {}), nullptr);
    snapshot.count = GrenadePlayerCollisionSnapshot::kCapacity + 1;
    EXPECT_EQ(grenade_player_collision_mirror::select(snapshot, {}), nullptr);
    snapshot.count = 1;
    EXPECT_EQ(grenade_player_collision_mirror::select(snapshot, {std::numeric_limits<float>::infinity(), 0.0f, 0.0f}), nullptr);
    snapshot.candidates[0].mins.x = 2.0f;
    EXPECT_EQ(grenade_player_collision_mirror::select(snapshot, {}), nullptr);
    snapshot.candidates[0] = {1, {-1.0f, -1.0f, -1.0f}, {std::numeric_limits<float>::infinity(), 1.0f, 1.0f}, true};
    EXPECT_EQ(grenade_player_collision_mirror::select(snapshot, {}), nullptr);
}

TEST(GrenadePlayerCollisionSelectionTest, CoCenteredNestedBoundsUseLowestHandle)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {100, {-10.0f, -10.0f, -10.0f}, {10.0f, 10.0f, 10.0f}, true};
    snapshot.candidates[snapshot.count++] = {1, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};

    const auto* const selected = grenade_player_collision_mirror::select(snapshot, {});

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->rawHandle, 1);
}

TEST(GrenadeSimulationParityTest, PreservesBaselinePlayerBounceResponse)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f}, true};
    simulator.setPlayerCollisionSnapshot(&snapshot);
    cs2::Vector position{1.0f, 0.0f, 0.0f};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_NEAR(velocity.x, -30.0f, 0.001f);
    EXPECT_NEAR(velocity.y, 0.0f, 0.001f);
    EXPECT_NEAR(velocity.z, -5.0f, 0.001f);
    EXPECT_NEAR(position.x, 0.53125f, 0.001f);
    EXPECT_NEAR(position.y, 0.0f, 0.001f);
    EXPECT_NEAR(position.z, -0.0390625f, 0.001f);
    EXPECT_EQ(context.trace.inFlightCalls, 2);
}

}
