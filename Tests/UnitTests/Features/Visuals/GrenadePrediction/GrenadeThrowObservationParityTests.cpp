#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadePredictionController.h>

namespace
{

TEST(GrenadeThrowObservationParityTest, FinalizesLegacyReleaseWithoutRetainedPinPulledStrength)
{
    GrenadePredictionState state;
    constexpr cs2::CEntityHandle weapon{1};
    ASSERT_TRUE(state.throwObservation.observeWeapon(weapon));
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.tagTempTrajectory(weapon, state.throwObservation.pendingSequence());
    EXPECT_FALSE(state.throwObservation.observePinState(weapon, true));
    EXPECT_TRUE(GrenadePredictionController::completeLegacyHeldThrow(state, weapon, state.throwObservation.observePinState(weapon, false), true, 10.0f));
    EXPECT_TRUE(state.throwObservation.isFinalized());
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
    EXPECT_FALSE(state.tempTrajectory.valid);
}

TEST(GrenadeThrowObservationParityTest, UnavailableThrowTimeDoesNotClearPendingExecution)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{1};
    ASSERT_TRUE(observation.observeWeapon(weapon));
    observation.retainThrowStrength(0.5f);
    ASSERT_TRUE(observation.observeThrowTime(weapon, 10.0f));

    const Optional<float> unavailableThrowTime;
    if (unavailableThrowTime.hasValue())
        static_cast<void>(observation.observeThrowTime(weapon, unavailableThrowTime.value()));
    EXPECT_TRUE(observation.hasPendingExecution());
    EXPECT_TRUE(observation.consumeActualExecution(true, 10.1f));
}

TEST(GrenadeThrowObservationParityTest, CapturesLateStrengthBeforeReadableThrowTimeLocksSequence)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{1};
    static_cast<void>(observation.observeWeapon(weapon));

    GrenadePredictionController::captureThrowStrength(observation, false, Optional<float>{10.0f}, []() noexcept { return Optional<float>{0.25f}; });
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.25f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
    EXPECT_TRUE(observation.observeThrowTime(weapon, 10.0f));

    GrenadePredictionController::captureThrowStrength(observation, true, Optional<float>{10.0f}, []() noexcept { return Optional<float>{0.5f}; });
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.25f);
}

TEST(GrenadeThrowObservationParityTest, CapturesPinnedZeroStrength)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{1};
    static_cast<void>(observation.observeWeapon(weapon));

    GrenadePredictionController::captureThrowStrength(observation, true, {}, []() noexcept { return Optional<float>{0.0f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.0f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadeThrowObservationParityTest, ActualExecutionWithoutRetainedStrengthDoesNotCommitTrajectory)
{
    GrenadePredictionState state;
    constexpr cs2::CEntityHandle weapon{1};
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    ASSERT_TRUE(state.throwObservation.observeWeapon(weapon));
    state.tagTempTrajectory(weapon, state.throwObservation.pendingSequence());
    ASSERT_TRUE(state.throwObservation.observeThrowTime(weapon, 10.0f));

    EXPECT_TRUE(GrenadePredictionController::completeHeldThrow(state, weapon, true, 10.1f));
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
    EXPECT_FALSE(state.tempTrajectory.valid);
}

TEST(GrenadeThrowObservationParityTest, CommitsExpiredThrowTimeInItsFirstObservation)
{
    GrenadePredictionState state;
    constexpr cs2::CEntityHandle weapon{1};
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    ASSERT_TRUE(state.throwObservation.observeWeapon(weapon));
    state.throwObservation.retainThrowStrength(0.5f);
    state.tagTempTrajectory(weapon, state.throwObservation.pendingSequence());
    ASSERT_TRUE(state.throwObservation.observeThrowTime(weapon, 10.0f));

    EXPECT_TRUE(GrenadePredictionController::completeHeldThrow(state, state.throwObservation.pendingWeapon(), true, 10.1f));
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
    EXPECT_FALSE(state.tempTrajectory.valid);
}

TEST(GrenadeThrowObservationParityTest, CommitsUsingPendingWeaponAcrossWeaponSwitch)
{
    GrenadePredictionState state;
    constexpr cs2::CEntityHandle oldWeapon{1};
    constexpr cs2::CEntityHandle newWeapon{2};
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    ASSERT_TRUE(state.throwObservation.observeWeapon(oldWeapon));
    state.throwObservation.retainThrowStrength(0.5f);
    state.tagTempTrajectory(oldWeapon, state.throwObservation.pendingSequence());
    ASSERT_TRUE(state.throwObservation.observeThrowTime(oldWeapon, 10.0f));

    EXPECT_TRUE(GrenadePredictionController::completeHeldThrow(state, state.throwObservation.pendingWeapon(), true, 10.1f));
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
    EXPECT_TRUE(state.throwObservation.observeWeapon(newWeapon));
}

}
