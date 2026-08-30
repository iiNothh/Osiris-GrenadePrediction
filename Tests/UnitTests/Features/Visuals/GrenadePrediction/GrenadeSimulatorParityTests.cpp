#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadeGravity.h>
#include <GameClient/EngineTrace/EngineTraceTypes.h>
#include <Mocks/GrenadePrediction/ScriptedGrenadeTrace.h>

namespace
{

using Simulator = GrenadeSimulator<GrenadeSimulatorTestHookContext>;

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
    simulator.simulate(trajectory, {{}, {100.0f, 0.0f, 0.0f}}, cs2::GrenadeKind::SmokeGrenade, nullptr, 800.0f);
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
    simulator.simulate(trajectory, {{}, {1.0e12f, 0.0f, 0.0f}}, cs2::GrenadeKind::SmokeGrenade, nullptr, 800.0f);
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
    simulator.simulate(trajectory, {{}, {100.0f, 0.0f, 0.0f}}, cs2::GrenadeKind::HEGrenade, nullptr, 800.0f);
    ASSERT_TRUE(trajectory.valid);
    ASSERT_EQ(trajectory.pointsCount, 54);
    EXPECT_NEAR(trajectory.points[1].x, 3.125f, 0.001f);
    EXPECT_NEAR(trajectory.points[1].z, -0.15625f, 0.001f);
    EXPECT_NEAR(trajectory.endPos.z, -430.6640625f, 0.001f);
}

TEST(GrenadeSimulationParityTest, UsesReferenceDecoyAndSmokeTimeoutTicks)
{
    EXPECT_FALSE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::shouldDetonate(cs2::GrenadeKind::Decoy, 640));
    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::shouldDetonate(cs2::GrenadeKind::Decoy, 641));
    EXPECT_FALSE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::shouldDetonate(cs2::GrenadeKind::SmokeGrenade, 1152));
    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::shouldDetonate(cs2::GrenadeKind::SmokeGrenade, 1153));
}

TEST(GrenadeSimulationParityTest, ContinuesTraceForTheUnusedCollisionSubstep)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.push(TraceResult{0.5f, {}, {-1.0f, 0.0f, 0.0f}, engine_trace::kWorldEntityHandle, true});
    context.trace.clearAfterScript();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{1000.0f, 0.0f, 0.0f};
    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, cs2::GrenadeKind::HEGrenade).traceSucceeded);
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
    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, cs2::GrenadeKind::HEGrenade, owner).traceSucceeded);
    EXPECT_EQ(velocity, (cs2::Vector{40.0f, 2.0f, -1.5f}));
    EXPECT_EQ(position, (cs2::Vector{4.46875f, 0.0234375f, 0.00390625f}));
    EXPECT_EQ(context.trace.calls, 3);
    EXPECT_EQ(context.trace.lastExcludedFirst, owner);
    EXPECT_EQ(context.trace.lastExcludedSecond, &context.entitySystem.entity);
}

TEST(GrenadeSimulationParityTest, UsesTheBaselineGenericInFlightTraceConfiguration)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, cs2::GrenadeKind::HEGrenade).traceSucceeded);
    EXPECT_EQ(context.trace.lastMask, grenade_prediction_params::kInFlightTraceMask);
    EXPECT_EQ(context.trace.lastCollisionGroup, grenade_prediction_params::kInFlightTraceCollisionGroup);
    EXPECT_EQ(context.trace.lastQueryByte, grenade_prediction_params::kInFlightTraceQueryByte);
}

TEST(GrenadeSimulationParityTest, RejectsAnInFlightHitWithoutARawEntityHandle)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.push(TraceResult{0.5f, {}, {-1.0f, 0.0f, 0.0f}});
    Simulator simulator{context};
    Trajectory trajectory;

    simulator.simulate(trajectory, {{}, {100.0f, 0.0f, 0.0f}}, cs2::GrenadeKind::HEGrenade, nullptr, 800.0f);

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

    EXPECT_FALSE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, cs2::GrenadeKind::HEGrenade).traceSucceeded);
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

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, cs2::GrenadeKind::HEGrenade);

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

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, cs2::GrenadeKind::HEGrenade);

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
        simulator, {0.5f, {}, {0.0f, 0.0f, 1.0f}, engine_trace::kWorldEntityHandle, true}, worldVelocity, cs2::GrenadeKind::HEGrenade));
    static_cast<void>(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::applyContactResponse(
        simulator, {0.5f, {}, {0.0f, 0.0f, 1.0f}, engine_trace::kWorldEntityHandle, false}, unknownVelocity, cs2::GrenadeKind::HEGrenade));
    EXPECT_NEAR(worldVelocity.z, 227.24025f, 0.001f);
    EXPECT_NEAR(unknownVelocity.z, 450.0140625f, 0.001f);
}

TEST(EngineTraceOutputValidationTest, RejectsRawHandleOffsetsOverlappingRequiredOutputs)
{
    constexpr auto endPositionOffset = 0x10;
    constexpr auto normalOffset = 0x20;
    constexpr auto fractionOffset = 0x30;

    EXPECT_TRUE(engine_trace::isValidRawEntityHandleOffset(0xBC, endPositionOffset, normalOffset, fractionOffset));
    EXPECT_FALSE(engine_trace::isValidRawEntityHandleOffset(0x18, endPositionOffset, normalOffset, fractionOffset));
    EXPECT_FALSE(engine_trace::isValidRawEntityHandleOffset(fractionOffset, endPositionOffset, normalOffset, fractionOffset));
}

TEST(EngineTraceDescriptorTest, UsesFixedGrenadeHullLayout)
{
    const engine_trace::FixedGrenadeHullTraceDescriptor descriptor{};
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(descriptor)>>(descriptor);
    constexpr std::array<std::byte, sizeof(descriptor)> expected{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0xC0},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0xC0},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0xC0},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x40},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x40},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x40},
        std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{},
        std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{},
        std::byte{0x02}, std::byte{}, std::byte{}, std::byte{},
        std::byte{}, std::byte{}, std::byte{}, std::byte{}
    };

    EXPECT_EQ(bytes, expected);
}

}
