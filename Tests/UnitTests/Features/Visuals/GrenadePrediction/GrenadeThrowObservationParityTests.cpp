#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadePredictionController.h>

namespace
{

TEST(GrenadeThrowObservationParityTest, CommitsLegacyReleaseOnlyWithRetainedPinPulledStrength)
{
    GrenadePredictionState state;
    const auto weapon = reinterpret_cast<const void*>(1);
    ASSERT_TRUE(state.throwObservation.observeWeapon(weapon));
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.tagTempTrajectory(weapon, state.throwObservation.pendingSequence());
    EXPECT_FALSE(GrenadePredictionController::completeLegacyHeldThrow(state, weapon, state.throwObservation.observePinState(weapon, false), true, 10.0f));
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);

    EXPECT_FALSE(state.throwObservation.observePinState(weapon, true));
    state.throwObservation.retainThrowStrength(0.5f);
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.tagTempTrajectory(weapon, state.throwObservation.pendingSequence());
    const bool releaseEdge = state.throwObservation.observePinState(weapon, false);
    EXPECT_TRUE(GrenadePredictionController::completeLegacyHeldThrow(state, weapon, releaseEdge, true, 10.0f));
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
}

TEST(GrenadeThrowObservationParityTest, UnavailableThrowTimeDoesNotClearPendingExecution)
{
    GrenadeThrowObservation observation;
    const auto weapon = reinterpret_cast<const void*>(1);
    ASSERT_TRUE(observation.observeWeapon(weapon));
    observation.retainThrowStrength(0.5f);
    ASSERT_TRUE(observation.observeThrowTime(weapon, 10.0f));

    const Optional<float> unavailableThrowTime;
    if (unavailableThrowTime.hasValue())
        static_cast<void>(observation.observeThrowTime(weapon, unavailableThrowTime.value()));
    EXPECT_TRUE(observation.hasPendingExecution());
    EXPECT_TRUE(observation.consumeActualExecution(true, 10.1f));
}

TEST(GrenadeThrowObservationParityTest, ActualExecutionWithoutRetainedStrengthDoesNotCommitTrajectory)
{
    GrenadePredictionState state;
    const int weapon{};
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    ASSERT_TRUE(state.throwObservation.observeWeapon(&weapon));
    state.tagTempTrajectory(&weapon, state.throwObservation.pendingSequence());
    ASSERT_TRUE(state.throwObservation.observeThrowTime(&weapon, 10.0f));

    EXPECT_TRUE(GrenadePredictionController::completeHeldThrow(state, &weapon, true, 10.1f));
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
}

TEST(GrenadeThrowObservationParityTest, CommitsExpiredThrowTimeInItsFirstObservation)
{
    GrenadePredictionState state;
    const int weapon{};
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    ASSERT_TRUE(state.throwObservation.observeWeapon(&weapon));
    state.throwObservation.retainThrowStrength(0.5f);
    state.tagTempTrajectory(&weapon, state.throwObservation.pendingSequence());
    ASSERT_TRUE(state.throwObservation.observeThrowTime(&weapon, 10.0f));

    EXPECT_TRUE(GrenadePredictionController::completeHeldThrow(state, state.throwObservation.pendingWeapon(), true, 10.1f));
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
}

TEST(GrenadeThrowObservationParityTest, CommitsUsingPendingWeaponAcrossWeaponSwitch)
{
    GrenadePredictionState state;
    const int oldWeapon{};
    const int newWeapon{};
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    ASSERT_TRUE(state.throwObservation.observeWeapon(&oldWeapon));
    state.throwObservation.retainThrowStrength(0.5f);
    state.tagTempTrajectory(&oldWeapon, state.throwObservation.pendingSequence());
    ASSERT_TRUE(state.throwObservation.observeThrowTime(&oldWeapon, 10.0f));

    EXPECT_TRUE(GrenadePredictionController::completeHeldThrow(state, state.throwObservation.pendingWeapon(), true, 10.1f));
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
    EXPECT_TRUE(state.throwObservation.observeWeapon(&newWeapon));
}

}
