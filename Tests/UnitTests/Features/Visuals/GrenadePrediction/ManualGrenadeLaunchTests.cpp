#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadeLaunchSelection.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeThrowObservation.h>
#include <Mocks/GrenadePrediction/ScriptedGrenadeTrace.h>

namespace
{

using Simulator = GrenadeSimulator<GrenadeSimulatorTestHookContext>;

TEST(ManualGrenadeLaunchTest, PrefersNativeLaunchWithoutCallingManualProvider)
{
    int nativeCalls{};
    int manualCalls{};
    const GrenadeLaunchState native{{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}};

    const auto prepared = prepareGrenadeLaunch(false, true, false, true,
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++nativeCalls; return native; },
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++manualCalls; return {}; });

    ASSERT_EQ(prepared.status, GrenadeLaunchPreparationStatus::Ready);
    ASSERT_TRUE(prepared.state.hasValue());
    EXPECT_EQ(prepared.state.value().origin, native.origin);
    EXPECT_EQ(prepared.state.value().velocity, native.velocity);
    EXPECT_EQ(nativeCalls, 1);
    EXPECT_EQ(manualCalls, 0);
}

TEST(ManualGrenadeLaunchTest, FallsBackToManualLaunchWhenNativeIsUnavailable)
{
    const GrenadeLaunchState manual{{7.0f, 8.0f, 9.0f}, {10.0f, 11.0f, 12.0f}};

    const auto prepared = prepareGrenadeLaunch(false, true, false, true,
        []() noexcept -> Optional<GrenadeLaunchState> { return {}; },
        [&]() noexcept -> Optional<GrenadeLaunchState> { return manual; });

    ASSERT_EQ(prepared.status, GrenadeLaunchPreparationStatus::Ready);
    ASSERT_TRUE(prepared.state.hasValue());
    EXPECT_EQ(prepared.state.value().origin, manual.origin);
    EXPECT_EQ(prepared.state.value().velocity, manual.velocity);
}

TEST(ManualGrenadeLaunchTest, UsesFullStrengthUntilPinnedStrengthIsCaptured)
{
    GrenadeThrowObservation observation;
    const int weapon{};
    static_cast<void>(observation.observeWeapon(&weapon));

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);

    static_cast<void>(observation.observePinState(&weapon, true));
    observation.retainThrowStrength(0.35f);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.35f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(ManualGrenadeLaunchTest, SkipsAvailableNativeLaunchUntilRealStrengthIsCaptured)
{
    GrenadeThrowObservation observation;
    const int weapon{};
    int nativeCalls{};
    int manualCalls{};
    static_cast<void>(observation.observeWeapon(&weapon));

    const auto manual = prepareGrenadeLaunch(false, true, false, observation.hasRetainedThrowStrength,
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++nativeCalls; return GrenadeLaunchState{}; },
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++manualCalls; return GrenadeLaunchState{{1.0f, 0.0f, 0.0f}, {observation.retainedThrowStrength, 0.0f, 0.0f}}; });

    ASSERT_TRUE(manual.state.hasValue());
    EXPECT_EQ(manual.status, GrenadeLaunchPreparationStatus::Ready);
    EXPECT_EQ(nativeCalls, 0);
    EXPECT_EQ(manualCalls, 1);
    EXPECT_FLOAT_EQ(manual.state.value().velocity.x, 1.0f);

    observation.retainThrowStrength(0.5f);
    const auto native = prepareGrenadeLaunch(false, true, false, observation.hasRetainedThrowStrength,
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++nativeCalls; return GrenadeLaunchState{{2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}}; },
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++manualCalls; return {}; });

    ASSERT_TRUE(native.state.hasValue());
    EXPECT_EQ(native.status, GrenadeLaunchPreparationStatus::Ready);
    EXPECT_EQ(nativeCalls, 1);
    EXPECT_EQ(manualCalls, 1);
    EXPECT_FLOAT_EQ(native.state.value().velocity.x, 3.0f);
}

TEST(ManualGrenadeLaunchTest, RetainsPinnedZeroStrength)
{
    GrenadeThrowObservation observation;
    const int weapon{};
    static_cast<void>(observation.observeWeapon(&weapon));
    static_cast<void>(observation.observePinState(&weapon, true));
    observation.retainThrowStrength(0.0f);

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.0f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(ManualGrenadeLaunchTest, ComputesReferenceSpawnAndInitialVelocity)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};

    const auto spawn = simulator.computeSpawnPosition({10.0f, 20.0f, 64.0f}, {}, 1.0f, nullptr);
    ASSERT_TRUE(spawn.hasValue());
    const auto forward = Simulator::forwardFromAngles(-10.0f, 0.0f);
    EXPECT_NEAR(spawn.value().x, 10.0f + forward.x * 16.0f, 0.001f);
    EXPECT_NEAR(spawn.value().y, 20.0f + forward.y * 16.0f, 0.001f);
    EXPECT_NEAR(spawn.value().z, 64.0f + forward.z * 16.0f, 0.001f);

    const auto velocity = Simulator::computeInitialVelocity({}, grenade_prediction_params::kBaseThrowVelocity, 1.0f);
    EXPECT_NEAR(velocity.x, 675.0f * forward.x, 0.001f);
    EXPECT_NEAR(velocity.z, 675.0f * forward.z, 0.001f);
}

}
