#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include <CS2/EngineTrace/CGameTrace.h>
#include <Features/Visuals/GrenadePrediction/GrenadeGravity.h>
#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionState.h>
#include <GameClient/Entities/GrenadeKind.h>
#include <GameClient/EngineTrace/TraceConversion.h>
#include <GameClient/EngineTrace/TraceResult.h>
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

[[nodiscard]] SmokeInfernoScan smokeInfernoScan() noexcept
{
    SmokeInfernoSnapshot inferno{
        .fireCount = 1,
        .fireLifetime = 1.0f
    };
    inferno.fireIsBurning[0] = true;
    SmokeInfernoScan scan;
    scan.beginScan();
    scan.observe(inferno);
    static_cast<void>(scan.endScan());
    return scan;
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
        context.trace.push(TraceResult{0.5f, {}, {-1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle});
        context.trace.pushClear();
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
    context.trace.push(TraceResult{0.5f, {}, {-1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle});
    context.trace.clearAfterScript();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{1000.0f, 0.0f, 0.0f};
    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade).traceSucceeded);
    EXPECT_EQ(context.trace.calls, 3);
}

class CapturedStoredOriginTest : public testing::TestWithParam<GrenadeKind> {
};

TEST_P(CapturedStoredOriginTest, RetainsCapturedStep686OriginWithoutRevertingVelocity)
{
    const cs2::Vector old{
        std::bit_cast<float>(0x432C7F79u), std::bit_cast<float>(0xC4B66CB4u), std::bit_cast<float>(0xC3CE9D6Du)};
    const cs2::Vector candidate{
        std::bit_cast<float>(0x432C7E90u), std::bit_cast<float>(0xC4B66CD0u), std::bit_cast<float>(0xC3CE9DD1u)};
    GrenadeSimulatorTestHookContext context;
    context.trace.push(TraceResult{1.0f, candidate, {}});
    context.trace.clearAfterScript();
    Simulator simulator{context};
    auto position = old;
    cs2::Vector velocity{
        std::bit_cast<float>(0xBEE8AE6Eu), std::bit_cast<float>(0xBEE3633Bu), std::bit_cast<float>(0x3F5C42C0u)};

    const auto first = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, GetParam());

    EXPECT_TRUE(first.traceSucceeded);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(position.x), 0x432C7F79u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(position.y), 0xC4B66CB4u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(position.z), 0xC3CE9D6Du);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity.x), 0xBEE8AE6Eu);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity.y), 0xBEE3633Bu);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity.z), 0xBFD1DEA0u);
    EXPECT_EQ(context.maxCoordReads, 1);

    const auto second = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, GetParam());

    EXPECT_TRUE(second.traceSucceeded);
    EXPECT_EQ(context.trace.lastStart, old);
    EXPECT_EQ(context.maxCoordReads, 2);
    EXPECT_EQ(position, context.trace.lastEnd);
}

INSTANTIATE_TEST_SUITE_P(GrenadeKinds, CapturedStoredOriginTest, testing::Values(
    GrenadeKind::SmokeGrenade, GrenadeKind::HEGrenade, GrenadeKind::Molotov));

TEST(GrenadeSimulationParityTest, ClearTraceUsesAuthoritativeEndpointIncludingZero)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.push(TraceResult{1.0f, {}, {}});
    Simulator simulator{context};
    cs2::Vector position{1.0f, 2.0f, 3.0f};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, GrenadeKind::HEGrenade);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_EQ(position, cs2::Vector{});
    EXPECT_NE(position, context.trace.lastEnd);
    EXPECT_EQ(context.maxCoordReads, 1);
}

TEST(GrenadeSimulationParityTest, ReadsCurrentMaxCoordForEachMovementCommit)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::Vector candidate{65536.015625f, 0.0f, 0.0f};
    context.trace.push(TraceResult{1.0f, candidate, {}});
    context.trace.push(TraceResult{1.0f, candidate, {}});
    Simulator simulator{context};
    cs2::Vector position{65536.0f, 0.0f, 0.0f};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, GrenadeKind::HEGrenade).traceSucceeded);
    EXPECT_EQ(position.x, 65536.0f);
    context.maxCoord = 32768.0f;
    EXPECT_TRUE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, GrenadeKind::HEGrenade).traceSucceeded);
    EXPECT_EQ(position, candidate);
    EXPECT_EQ(context.maxCoordReads, 2);
}

struct ReaderlessMovementContext {
    ScriptedGrenadeTrace trace;

    template <template <typename> typename T>
        requires std::is_same_v<T<ReaderlessMovementContext>, EngineTrace<ReaderlessMovementContext>>
    [[nodiscard]] ScriptedGrenadeTrace& make() noexcept
    {
        return trace;
    }
};

TEST(GrenadeSimulationParityTest, ContextWithoutScaleCapabilityFailsMovementButStillSupportsResponseOnly)
{
    ReaderlessMovementContext context;
    context.trace.clearAfterScript();
    GrenadeSimulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    const auto movement = GrenadeSimulatorTestAccess<ReaderlessMovementContext>::movementSubstep(simulator, position, velocity, GrenadeKind::HEGrenade);
    EXPECT_FALSE(movement.traceSucceeded);
    EXPECT_EQ(position, cs2::Vector{});
    const auto response = GrenadeSimulatorTestAccess<ReaderlessMovementContext>::applyContactResponse(
        simulator, {0.5f, {}, {-1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle}, velocity, GrenadeKind::HEGrenade);
    EXPECT_TRUE(response.traceSucceeded);
    EXPECT_LT(velocity.x, 0.0f);
}

class MovementOriginUnavailableTest : public testing::TestWithParam<Optional<float>> {
};

TEST_P(MovementOriginUnavailableTest, InvalidatesTrajectoryWithoutDefaultScale)
{
    GrenadeSimulatorTestHookContext context;
    context.maxCoord = GetParam();
    context.trace.clearAfterScript();
    Simulator simulator{context};
    Trajectory trajectory;
    const cs2::Vector origin{1.0f, 2.0f, 3.0f};

    simulator.simulate(trajectory, {origin, {100.0f, 0.0f, 0.0f}}, GrenadeKind::HEGrenade, nullptr, 800.0f);

    EXPECT_FALSE(trajectory.valid);
    EXPECT_EQ(trajectory.pointsCount, 0);
    EXPECT_EQ(trajectory.markersCount, 0);
    EXPECT_EQ(trajectory.endPos, origin);
    EXPECT_EQ(context.maxCoordReads, 1);
    EXPECT_EQ(context.trace.calls, 1);
}

INSTANTIATE_TEST_SUITE_P(MissingOrInvalid, MovementOriginUnavailableTest, testing::Values(
    Optional<float>{}, Optional<float>{0.0f}, Optional<float>{-0.0f}, Optional<float>{-1.0f},
    Optional<float>{std::numeric_limits<float>::infinity()}, Optional<float>{std::numeric_limits<float>::quiet_NaN()}));

class ZeroFractionMovementOriginTest : public testing::TestWithParam<float> {
};

TEST_P(ZeroFractionMovementOriginTest, RespondsToRealContactWithoutReadingScaleOrMovingOrigin)
{
    GrenadeSimulatorTestHookContext context;
    context.maxCoord = {};
    const cs2::Vector contact{9.0f, 8.0f, 7.0f};
    context.trace.push(TraceResult{GetParam(), contact, {-1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle});
    Simulator simulator{context};
    cs2::Vector position{1.0f, 2.0f, 3.0f};
    cs2::Vector velocity{1.0f, 0.0f, 0.0f};
    Trajectory trajectory;

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(
        simulator, position, velocity, GrenadeKind::HEGrenade, nullptr, 800.0f, &trajectory);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_TRUE(result.hit);
    EXPECT_EQ(result.contactsCount, 1);
    EXPECT_EQ(position, (cs2::Vector{1.0f, 2.0f, 3.0f}));
    EXPECT_EQ(velocity, cs2::Vector{});
    EXPECT_EQ(context.maxCoordReads, 0);
    ASSERT_EQ(trajectory.pointsCount, 1);
    EXPECT_EQ(trajectory.points[0], contact);
    EXPECT_EQ(trajectory.worldContactMarkersCount, 1);
}

INSTANTIATE_TEST_SUITE_P(SignedZero, ZeroFractionMovementOriginTest, testing::Values(0.0f, -0.0f));

TEST(GrenadeSimulationParityTest, ZeroFractionResidualSkipsCommitAndScaleRead)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::Vector contact{4.0f, 5.0f, 6.0f};
    context.trace.push(TraceResult{0.5f, contact, {-1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle});
    context.trace.push(TraceResult{-0.0f, {9.0f, 8.0f, 7.0f}, {}, cs2::engine_trace::kWorldEntityHandle});
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, GrenadeKind::HEGrenade);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_EQ(position, contact);
    EXPECT_EQ(context.maxCoordReads, 1);
    EXPECT_EQ(context.trace.calls, 2);
}

TEST(GrenadeSimulationParityTest, PrimaryRetainsOriginButContactMarkerUsesRawEndpointAndResidualStartsRetained)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::Vector old{10.0f, 20.0f, 30.0f};
    const cs2::Vector contact = old + cs2::Vector{0.001f, -0.002f, 0.003f};
    context.trace.push(TraceResult{0.5f, contact, {-1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle});
    context.trace.pushClear();
    Simulator simulator{context};
    auto position = old;
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};
    Trajectory trajectory;

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(
        simulator, position, velocity, GrenadeKind::HEGrenade, nullptr, 800.0f, &trajectory);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_EQ(result.contactsCount, 1);
    EXPECT_EQ(context.trace.starts[1], old);
    EXPECT_EQ(position, context.trace.ends[1]);
    EXPECT_EQ(context.maxCoordReads, 2);
    ASSERT_EQ(trajectory.pointsCount, 1);
    EXPECT_EQ(trajectory.points[0], contact);
    EXPECT_EQ(trajectory.worldContactMarkersCount, 1);
    EXPECT_NEAR(velocity.x, -45.0140625f, 0.001f);
}

TEST(GrenadeSimulationParityTest, ResidualRetentionCarriesIntoNextSubstepInsteadOfRollingBackPrimary)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::Vector contact{4.0f, 5.0f, 6.0f};
    context.trace.push(TraceResult{0.5f, contact, {-1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle});
    context.trace.push(TraceResult{1.0f, contact + cs2::Vector{0.001f, 0.002f, -0.003f}, {}});
    context.trace.pushClear();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_EQ(context.trace.starts[1], contact);
    EXPECT_EQ(context.trace.starts[2], contact);
    EXPECT_EQ(position, context.trace.ends[2]);
    EXPECT_EQ(context.maxCoordReads, 3);
}

TEST(GrenadeSimulationParityTest, FailedSmokePlacementDoesNotOverwriteRetainedOriginWithRawContact)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::Vector old{1.0f, 2.0f, 3.0f};
    const cs2::Vector contact = old + cs2::Vector{0.001f, -0.002f, 0.003f};
    context.trace.push(TraceResult{0.5f, contact, {-1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle});
    context.trace.push(Optional<TraceResult>{});
    Simulator simulator{context};
    const auto scan = smokeInfernoScan();
    simulator.setSmokeInfernoScan(&scan);
    auto position = old;
    cs2::Vector velocity{1.0f, 0.0f, 0.0f};

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, GrenadeKind::SmokeGrenade);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_FALSE(result.smokePlacementComplete);
    EXPECT_EQ(position, old);
    EXPECT_EQ(context.trace.lastEnd, contact);
    EXPECT_EQ(context.maxCoordReads, 1);
    EXPECT_EQ(context.trace.calls, 2);
}

TEST(GrenadeSimulationParityTest, SuccessfulSmokeCorrectionBypassesMovementGateAndStartsFromRawContact)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::Vector old{1.0f, 2.0f, 3.0f};
    const cs2::Vector contact = old + cs2::Vector{0.001f, -0.002f, 0.003f};
    const cs2::Vector corrected = old + cs2::Vector{0.002f, 0.001f, -0.003f};
    context.trace.push(TraceResult{0.5f, contact, {-1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle});
    context.trace.pushClear();
    context.trace.push(TraceResult{0.25f, corrected, {}});
    Simulator simulator{context};
    const auto scan = smokeInfernoScan();
    simulator.setSmokeInfernoScan(&scan);
    auto position = old;
    cs2::Vector velocity{1.0f, 0.0f, 0.0f};
    Trajectory trajectory;

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(
        simulator, position, velocity, GrenadeKind::SmokeGrenade, nullptr, 800.0f, &trajectory);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_TRUE(result.smokePlacementComplete);
    EXPECT_EQ(position, corrected);
    EXPECT_EQ(context.trace.lastStart, contact);
    EXPECT_EQ(context.maxCoordReads, 1);
    ASSERT_EQ(trajectory.pointsCount, 2);
    EXPECT_EQ(trajectory.points[0], contact);
    EXPECT_EQ(trajectory.points[1], corrected);
    EXPECT_EQ(trajectory.worldContactMarkersCount, 1);
}

TEST(GrenadeSimulationParityTest, DynamicPaneRetainsNearPrimaryOriginAndDampsOnceWithoutResidual)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::CEntityHandle pane{42u};
    context.entitySystem.setDynamicProp(pane);
    const cs2::Vector old{1.0f, 2.0f, 3.0f};
    context.trace.push(TraceResult{0.5f, old + cs2::Vector{0.001f, -0.002f, 0.003f},
        {-1.0f, 0.0f, 0.0f}, static_cast<std::int32_t>(pane.value)});
    context.trace.pushClear();
    Simulator simulator{context};
    auto position = old;
    cs2::Vector velocity{100.0f, 5.0f, 5.0f};

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, GrenadeKind::HEGrenade);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_TRUE(result.hit);
    EXPECT_EQ(result.contactsCount, 0);
    EXPECT_EQ(position, old);
    EXPECT_EQ(velocity, (cs2::Vector{40.0f, 2.0f, 1.0f}));
    EXPECT_EQ(context.trace.calls, 1);
    EXPECT_EQ(context.maxCoordReads, 1);
}

TEST(GrenadeSimulationParityTest, DampsDynamicPaneAndExcludesItFromLaterTraces)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::CEntityHandle pane{42u};
    context.entitySystem.setDynamicProp(pane);
    context.trace.push(TraceResult{0.5f, {4.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, static_cast<std::int32_t>(pane.value)});
    context.trace.clearAfterScript();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 5.0f, 5.0f};
    void* const owner = &context;
    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade, owner);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_TRUE(result.hit);
    EXPECT_EQ(result.contactsCount, 0);
    EXPECT_FALSE(result.impactDetonate);
    EXPECT_FALSE(result.smokePlacementComplete);
    EXPECT_EQ(velocity, (cs2::Vector{40.0f, 2.0f, -1.5f}));
    EXPECT_EQ(position, (cs2::Vector{4.3125f, 0.015625f, -0.001953125f}));
    EXPECT_EQ(context.trace.calls, 2);
    EXPECT_EQ(context.trace.inFlightCalls, 2);
    EXPECT_EQ(context.trace.lastStart, (cs2::Vector{4.0f, 0.0f, 0.0f}));
    EXPECT_EQ(context.trace.lastEnd, position);
    EXPECT_EQ(context.trace.lastExcludedFirst, owner);
    EXPECT_EQ(context.trace.lastExcludedSecond, &context.entitySystem.entity);
}

class CapturedDynamicPanePassageTest : public testing::TestWithParam<GrenadeKind> {
};

TEST_P(CapturedDynamicPanePassageTest, ReturnsAtCapturedContactWithoutResidualTrace)
{
    constexpr cs2::Vector contact{
        std::bit_cast<float>(std::uint32_t{0x44870C4C}),
        std::bit_cast<float>(std::uint32_t{0xC447A200}),
        std::bit_cast<float>(std::uint32_t{0xC3A4CC6B})};
    constexpr cs2::Vector incoming{
        std::bit_cast<float>(std::uint32_t{0x42770A89}),
        std::bit_cast<float>(std::uint32_t{0x44268FB5}),
        std::bit_cast<float>(std::uint32_t{0xC10765A0})};
    constexpr float gravityStep = grenade_prediction_params::kDefaultServerGravity
        * grenade_prediction_params::kGravityScale * grenade_prediction_params::kMovementSubstepDt;

    GrenadeSimulatorTestHookContext context;
    const cs2::CEntityHandle pane{42u};
    context.entitySystem.setDynamicProp(pane);
    context.trace.push(TraceResult{std::bit_cast<float>(std::uint32_t{0x3F3343E0}), contact,
        {0.0f, -1.0f, 0.0f}, static_cast<std::int32_t>(pane.value)});
    context.trace.push(TraceResult{1.0f, {}, {}});
    Simulator simulator{context};
    const auto scan = smokeInfernoScan();
    simulator.setSmokeInfernoScan(&scan);
    cs2::Vector position{};
    cs2::Vector velocity{incoming.x, incoming.y, incoming.z + gravityStep};

    ASSERT_EQ(std::bit_cast<std::uint32_t>(velocity.x), 0x42770A89u);
    ASSERT_EQ(std::bit_cast<std::uint32_t>(velocity.y), 0x44268FB5u);
    ASSERT_EQ(std::bit_cast<std::uint32_t>(velocity.z - gravityStep), 0xC10765A0u);

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, GetParam());

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_TRUE(result.hit);
    EXPECT_EQ(result.contactsCount, 0);
    EXPECT_FALSE(result.impactDetonate);
    EXPECT_FALSE(result.smokePlacementComplete);
    EXPECT_EQ(context.trace.calls, 1);
    EXPECT_EQ(context.trace.inFlightCalls, 1);
    EXPECT_EQ(context.trace.pointHullCalls, 0);
    EXPECT_EQ(context.trace.resultCount, 2);
    EXPECT_EQ(context.trace.lastStart, cs2::Vector{});
    EXPECT_EQ(std::bit_cast<std::uint32_t>(context.trace.lastEnd.x), 0x3EF70A89u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(context.trace.lastEnd.y), 0x40A68FB5u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(context.trace.lastEnd.z), 0xBD66CB40u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(position.x), 0x44870C4Cu);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(position.y), 0xC447A200u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(position.z), 0xC3A4CC6Bu);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity.x), 0x41C5A207u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity.y), 0x43853FC4u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity.z), 0xC058A29Au);
}

INSTANTIATE_TEST_SUITE_P(GrenadeKinds, CapturedDynamicPanePassageTest, testing::Values(
    GrenadeKind::HEGrenade, GrenadeKind::Molotov, GrenadeKind::SmokeGrenade));

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

TEST(GrenadeSimulationParityTest, ResolvesInFlightTraceBindingsOncePerSimulation)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};
    Trajectory trajectory;

    simulator.simulate(trajectory, {{}, {100.0f, 0.0f, 0.0f}}, GrenadeKind::HEGrenade, nullptr, 800.0f);

    EXPECT_TRUE(trajectory.valid);
    EXPECT_EQ(context.trace.inFlightBindingResolutionCalls, 1);
    EXPECT_GT(context.trace.inFlightCalls, 1);
}

TEST(GrenadeSimulationParityTest, ResolvesFreshInFlightTraceBindingsForEachSimulation)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};
    Trajectory trajectory;

    simulator.simulate(trajectory, {{}, {100.0f, 0.0f, 0.0f}}, GrenadeKind::HEGrenade, nullptr, 800.0f);
    simulator.simulate(trajectory, {{}, {100.0f, 0.0f, 0.0f}}, GrenadeKind::HEGrenade, nullptr, 800.0f);

    EXPECT_TRUE(trajectory.valid);
    EXPECT_EQ(context.trace.inFlightBindingResolutionCalls, 2);
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
    EXPECT_EQ(context.trace.inFlightBindingResolutionCalls, 1);
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
    context.trace.push(TraceResult{0.5f, {4.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, static_cast<std::int32_t>(pane.value)});
    context.trace.push(TraceResult{0.5f, {4.1f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, static_cast<std::int32_t>(pane.value)});
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    EXPECT_FALSE(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade).traceSucceeded);
    EXPECT_EQ(context.trace.calls, 2);
    EXPECT_EQ(context.trace.lastStart, (cs2::Vector{4.0f, 0.0f, 0.0f}));
    EXPECT_EQ(context.trace.lastEnd, (cs2::Vector{4.3125f, 0.0f, -0.017578125f}));
    EXPECT_EQ(context.trace.lastExcludedSecond, &context.entitySystem.entity);
    EXPECT_EQ(position, (cs2::Vector{4.0f, 0.0f, 0.0f}));
    EXPECT_EQ(velocity, (cs2::Vector{40.0f, 0.0f, -3.5f}));
}

TEST(GrenadeSimulationParityTest, AppliesOrdinaryWorldResponseAndResidualTraceOnSubstepAfterPanePassage)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::CEntityHandle pane{42u};
    context.entitySystem.setDynamicProp(pane);
    context.trace.push(TraceResult{0.5f, {4.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, static_cast<std::int32_t>(pane.value)});
    context.trace.push(TraceResult{0.5f, {4.1f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle});
    context.trace.clearAfterScript();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{200.0f, 0.0f, 0.0f};

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_TRUE(result.hit);
    EXPECT_EQ(result.contactsCount, 1);
    EXPECT_FALSE(result.impactDetonate);
    EXPECT_FALSE(result.smokePlacementComplete);
    EXPECT_EQ(context.trace.calls, 3);
    EXPECT_EQ(context.trace.lastStart, (cs2::Vector{4.1f, 0.0f, 0.0f}));
    EXPECT_EQ(context.trace.lastEnd, (cs2::Vector{4.1f, 0.0f, 0.0f}
        + velocity * (0.5f * grenade_prediction_params::kMovementSubstepDt)));
    EXPECT_EQ(position, context.trace.lastEnd);
    EXPECT_EQ(context.trace.lastExcludedSecond, &context.entitySystem.entity);
    EXPECT_NEAR(velocity.x, -36.0140625f, 0.001f);
    EXPECT_NEAR(velocity.z, -1.575f, 0.001f);
}

TEST(GrenadeSimulationParityTest, CompletesSmokeAtFirstRealWorldContactAndPreservesContactMarker)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.push(TraceResult{0.5f, {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle});
    context.trace.pushClear();
    context.trace.push(TraceResult{0.25f, {4.0f, 5.0f, 6.0f}, {}});
    Simulator simulator{context};
    const auto scan = smokeInfernoScan();
    simulator.setSmokeInfernoScan(&scan);
    Trajectory trajectory;

    simulator.simulate(trajectory, {{}, {100.0f, 0.0f, 0.0f}}, GrenadeKind::SmokeGrenade, nullptr, 800.0f);

    ASSERT_TRUE(trajectory.valid);
    EXPECT_EQ(trajectory.endPos, (cs2::Vector{4.0f, 5.0f, 6.0f}));
    ASSERT_EQ(trajectory.pointsCount, 3);
    EXPECT_EQ(trajectory.points[1], (cs2::Vector{1.0f, 0.0f, 0.0f}));
    EXPECT_EQ(trajectory.points[2], trajectory.endPos);
    ASSERT_EQ(trajectory.markersCount, 1);
    EXPECT_EQ(trajectory.markers[0].pointIndex, 1);
    EXPECT_EQ(context.trace.calls, 3);
    EXPECT_EQ(context.trace.inFlightBindingResolutionCalls, 1);
}

TEST(GrenadeSimulationParityTest, RetriesSmokePlacementAtLaterRealContactAfterFailedAttempt)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.push(TraceResult{0.5f, {}, {-1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle});
    context.trace.push(TraceResult{0.5f, {}, {0.0f, 0.0f, 1.0f}});
    context.trace.push(TraceResult{0.5f, {}, {0.0f, 0.0f, 1.0f}});
    context.trace.push(Optional<TraceResult>{});
    context.trace.pushClear();
    context.trace.push(TraceResult{0.5f, {}, {1.0f, 0.0f, 0.0f}, cs2::engine_trace::kWorldEntityHandle});
    context.trace.pushClear();
    context.trace.push(TraceResult{0.25f, {4.0f, 5.0f, 6.0f}, {}});
    Simulator simulator{context};
    const auto scan = smokeInfernoScan();
    simulator.setSmokeInfernoScan(&scan);
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    const auto first = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, GrenadeKind::SmokeGrenade);
    const auto second = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::movementSubstep(simulator, position, velocity, GrenadeKind::SmokeGrenade);

    EXPECT_TRUE(first.traceSucceeded);
    EXPECT_FALSE(first.smokePlacementComplete);
    EXPECT_TRUE(second.smokePlacementComplete);
    EXPECT_EQ(position, (cs2::Vector{4.0f, 5.0f, 6.0f}));
    EXPECT_EQ(context.trace.calls, 8);
}

TEST(GrenadeSimulationParityTest, DoesNotSearchForSmokePlacementAtFuseOnlyTermination)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};
    const auto scan = smokeInfernoScan();
    simulator.setSmokeInfernoScan(&scan);
    Trajectory trajectory;

    simulator.simulate(trajectory, {{}, {100.0f, 0.0f, 0.0f}}, GrenadeKind::SmokeGrenade, nullptr, 800.0f);

    EXPECT_TRUE(trajectory.valid);
    EXPECT_EQ(context.trace.pointHullCalls, 0);
}

TEST(GrenadeSimulationParityTest, AppliesAnOrdinaryResponseToAResolvedNonDynamicEntity)
{
    GrenadeSimulatorTestHookContext context;
    const cs2::CEntityHandle entity{43u};
    context.entitySystem.setNonDynamicProp(entity);
    context.trace.push(TraceResult{0.5f, {4.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, static_cast<std::int32_t>(entity.value)});
    context.trace.clearAfterScript();
    Simulator simulator{context};
    cs2::Vector position{};
    cs2::Vector velocity{100.0f, 0.0f, 0.0f};

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::step(simulator, position, velocity, GrenadeKind::HEGrenade);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_EQ(result.contactsCount, 1);
    EXPECT_LT(velocity.x, 0.0f);
}

struct CapturedSmokeContactResponse {
    const char* capture;
    std::array<std::uint32_t, 3> incomingBits;
    std::array<std::uint32_t, 3> normalBits;
    std::uint32_t fractionBits;
    std::array<std::uint32_t, 3> contactBits;
    std::array<std::uint32_t, 3> outgoingBits;
};

constexpr CapturedSmokeContactResponse kCapturedSmokeContactResponses[]{
    {
        "Shot1Contact3Substep187",
        {0xC2F31587u, 0x4238F757u, 0xC2ED1CDAu},
        {0x34DFF0D0u, 0xBF3504CBu, 0x3F35051Cu},
        0x3F209679u,
        {0x4352D2E7u, 0xC4B2E227u, 0xC2B5A934u},
        {0xC25AC687u, 0xC25570D7u, 0x41A68D67u}
    },
    {
        "Shot2Contact6Substep328",
        {0x4098CD09u, 0x41CB6415u, 0xC37FCDAAu},
        {0xB4425696u, 0xBE9D2F78u, 0x3F73A32Du},
        0x3F0E8EB4u,
        {0x440163ADu, 0x43D83302u, 0xC4053DEEu},
        {0x400984A9u, 0xC267F1C0u, 0x42C8392Au}
    }
};
static_assert(grenade_prediction_params::kElasticity == 0.45f);
static_assert(grenade_prediction_params::kClipPushOff == 0.03125f);

class CapturedSmokeContactResponseTest : public testing::TestWithParam<CapturedSmokeContactResponse> {
};

TEST_P(CapturedSmokeContactResponseTest, MatchesFullServerOutgoingBitsUsingZyxClippingSum)
{
    const auto& capture = GetParam();
    SCOPED_TRACE(capture.capture);
    const auto vectorFromBits = [](const std::array<std::uint32_t, 3>& bits) {
        return cs2::Vector{std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
    };
    GrenadeSimulatorTestHookContext context;
    Simulator simulator{context};
    auto velocity = vectorFromBits(capture.incomingBits);
    const TraceResult trace{std::bit_cast<float>(capture.fractionBits), vectorFromBits(capture.contactBits),
        vectorFromBits(capture.normalBits), cs2::engine_trace::kWorldEntityHandle};

    const auto result = GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::applyContactResponse(
        simulator, trace, velocity, GrenadeKind::SmokeGrenade);

    EXPECT_TRUE(result.traceSucceeded);
    EXPECT_FALSE(result.stopped);
    EXPECT_FALSE(result.impactDetonate);
    EXPECT_FALSE(result.smokePlacementComplete);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity.x), capture.outgoingBits[0]);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity.y), capture.outgoingBits[1]);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity.z), capture.outgoingBits[2]);
    EXPECT_EQ(context.trace.calls, 0);
}

INSTANTIATE_TEST_SUITE_P(CapturedRoutes, CapturedSmokeContactResponseTest, testing::ValuesIn(kCapturedSmokeContactResponses));

TEST(GrenadeSimulationParityTest, AppliesSteepFloorDampingOnlyWhenWorldHandleIsRead)
{
    GrenadeSimulatorTestHookContext context;
    Simulator simulator{context};
    cs2::Vector worldVelocity{100.0f, 0.0f, -1000.0f};
    cs2::Vector unknownVelocity = worldVelocity;
    static_cast<void>(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::applyContactResponse(
        simulator, {0.5f, {}, {0.0f, 0.0f, 1.0f}, cs2::engine_trace::kWorldEntityHandle}, worldVelocity, GrenadeKind::HEGrenade));
    static_cast<void>(GrenadeSimulatorTestAccess<GrenadeSimulatorTestHookContext>::applyContactResponse(
        simulator, {0.5f, {}, {0.0f, 0.0f, 1.0f}}, unknownVelocity, GrenadeKind::HEGrenade));
    EXPECT_NEAR(worldVelocity.z, 227.24025f, 0.001f);
    EXPECT_NEAR(unknownVelocity.z, 450.0140625f, 0.001f);
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
