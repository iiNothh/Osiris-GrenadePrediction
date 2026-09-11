#include <gtest/gtest.h>

#include <limits>

#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeLaunchSelection.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeThrowObservation.h>

namespace
{

TEST(GrenadePredictionThrowObservationTest, CommitsOnlyTheOwnedCompletedSequence)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};
    static_cast<void>(observation.observeWeapon(weapon));
    EXPECT_FALSE(observation.observePinState(weapon, true));
    EXPECT_TRUE(observation.observePinState(weapon, false));
    EXPECT_TRUE(observation.observeThrowTime(weapon, 10.0f));
    EXPECT_FALSE(observation.consumeActualExecution(true, 10.0f));
    EXPECT_TRUE(observation.consumeActualExecution(true, 10.1f));
    EXPECT_TRUE(observation.isFinalized());
}

TEST(GrenadePredictionThrowObservationTest, ClearsFinalizedSequenceWhenThrowTimeResets)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};
    static_cast<void>(observation.observeWeapon(weapon));
    observation.retainThrowStrength(0.5f);
    ASSERT_TRUE(observation.observeThrowTime(weapon, 10.0f));
    ASSERT_TRUE(observation.consumeActualExecution(true, 10.1f));
    const auto finalizedSequence = observation.pendingSequence();

    EXPECT_TRUE(observation.observeThrowTime(weapon, 0.0f));
    EXPECT_FALSE(observation.isFinalized());
    EXPECT_FALSE(observation.isStrengthLocked());
    EXPECT_FALSE(observation.hasPendingExecution());
    EXPECT_NE(observation.pendingSequence(), finalizedSequence);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionThrowObservationTest, PreparesLaunchWhenSameWeaponIsImmediatelyReequipped)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};
    int manualCalls{};
    static_cast<void>(observation.observeWeapon(weapon));
    observation.retainThrowStrength(0.5f);
    ASSERT_TRUE(observation.observeThrowTime(weapon, 10.0f));
    ASSERT_TRUE(observation.consumeActualExecution(true, 10.1f));

    ASSERT_TRUE(observation.observeThrowTime(weapon, 0.0f));
    const auto launch = prepareGrenadeLaunch(observation.isFinalized(), observation.hasRetainedThrowStrength,
        []() noexcept -> Optional<GrenadeLaunchState> { return {}; },
        [&]() noexcept -> Optional<GrenadeLaunchState> {
            ++manualCalls;
            return GrenadeLaunchState{};
        });

    EXPECT_TRUE(launch.hasValue());
    EXPECT_EQ(manualCalls, 1);
}

TEST(GrenadePredictionThrowObservationTest, ClearsPendingExecutionWithoutResettingItsSequence)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};
    static_cast<void>(observation.observeWeapon(weapon));
    observation.retainThrowStrength(0.5f);
    ASSERT_TRUE(observation.observeThrowTime(weapon, 10.0f));
    const auto pendingSequence = observation.pendingSequence();

    EXPECT_TRUE(observation.observeThrowTime(weapon, 0.0f));
    EXPECT_FALSE(observation.hasPendingExecution());
    EXPECT_FALSE(observation.isFinalized());
    EXPECT_FALSE(observation.isStrengthLocked());
    EXPECT_EQ(observation.pendingSequence(), pendingSequence);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.5f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionThrowObservationTest, TreatsNonFiniteThrowAndCurrentTimesAsUnavailable)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};
    static_cast<void>(observation.observeWeapon(weapon));

    EXPECT_FALSE(observation.observeThrowTime(weapon, std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(observation.hasPendingExecution());
    EXPECT_TRUE(observation.observeThrowTime(weapon, 10.0f));
    EXPECT_FALSE(observation.consumeActualExecution(true, std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(observation.isFinalized());
}

TEST(GrenadePredictionThrowObservationTest, UsesDefaultStrengthBeforePinIsPulled)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};

    EXPECT_TRUE(observation.observeWeapon(weapon));
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
}

TEST(GrenadeThrowObservationTest, CapturesRealStrengthAfterObservingPinPull)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};
    static_cast<void>(observation.observeWeapon(weapon));

    static_cast<void>(observation.observePinState(weapon, false));
    observation.captureThrowStrength(false, {}, []() noexcept { return Optional<float>{0.25f}; });
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);

    static_cast<void>(observation.observePinState(weapon, true));
    observation.captureThrowStrength(true, {}, []() noexcept { return Optional<float>{0.25f}; });
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.25f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadeThrowObservationTest, ResetsCompletedPartialFlashThrowWhenReacquiringFlashbang)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle thrownFlashbang{4};
    constexpr cs2::CEntityHandle reacquiredFlashbang{5};

    static_cast<void>(observation.observePinState(thrownFlashbang, true));
    observation.captureThrowStrength(true, {}, []() noexcept { return Optional<float>{0.5f}; });
    ASSERT_TRUE(observation.observeThrowTime(thrownFlashbang, 10.0f));
    ASSERT_TRUE(observation.consumeActualExecution(true, 10.1f));

    static_cast<void>(observation.observePinState(reacquiredFlashbang, false));
    observation.captureThrowStrength(false, {}, []() noexcept { return Optional<float>{0.0f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
    EXPECT_FALSE(observation.isFinalized());
}

TEST(GrenadeThrowObservationTest, ResetsPartialFlashThrowWhenObservingAnotherGrenadeKind)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle flashbang{4};
    constexpr cs2::CEntityHandle heGrenade{5};

    static_cast<void>(observation.observePinState(flashbang, true));
    observation.captureThrowStrength(true, {}, []() noexcept { return Optional<float>{0.5f}; });
    ASSERT_TRUE(observation.observeThrowTime(flashbang, 10.0f));

    static_cast<void>(observation.observePinState(heGrenade, false));
    observation.captureThrowStrength(false, {}, []() noexcept { return Optional<float>{0.0f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
    EXPECT_FALSE(observation.hasPendingExecution());
}

TEST(GrenadeThrowObservationTest, ResetsPendingStrengthForANewObservationOfTheSameKind)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle firstFlashbang{4};
    constexpr cs2::CEntityHandle secondFlashbang{5};

    static_cast<void>(observation.observePinState(firstFlashbang, true));
    observation.captureThrowStrength(true, {}, []() noexcept { return Optional<float>{0.5f}; });
    ASSERT_TRUE(observation.observeThrowTime(firstFlashbang, 10.0f));

    static_cast<void>(observation.observePinState(secondFlashbang, false));
    observation.captureThrowStrength(false, {}, []() noexcept { return Optional<float>{0.0f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
    EXPECT_FALSE(observation.hasPendingExecution());
}

TEST(GrenadeThrowObservationTest, RetainsPinnedZeroStrengthForTheCurrentThrow)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle flashbang{4};

    static_cast<void>(observation.observePinState(flashbang, true));
    observation.captureThrowStrength(true, {}, []() noexcept { return Optional<float>{0.0f}; });
    ASSERT_TRUE(observation.observeThrowTime(flashbang, 10.0f));
    static_cast<void>(observation.observePinState(flashbang, true));
    observation.captureThrowStrength(true, {}, []() noexcept { return Optional<float>{0.5f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.0f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadeThrowObservationTest, DoesNotReadZeroStrengthForAnUnpinnedFlashbang)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle flashbang{4};
    bool readThrowStrength{};

    static_cast<void>(observation.observePinState(flashbang, false));
    observation.captureThrowStrength(false, {}, [&]() noexcept {
        readThrowStrength = true;
        return Optional<float>{0.0f};
    });

    EXPECT_FALSE(readThrowStrength);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
}

TEST(GrenadeThrowObservationTest, DoesNotReadZeroStrengthForUnpinnedGenericGrenadeTransitions)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapons[]{{4}, {5}, {6}, {7}, {8}};

    for (const auto weapon : weapons) {
        bool readThrowStrength{};
        static_cast<void>(observation.observePinState(weapon, false));
        observation.captureThrowStrength(false, {}, [&]() noexcept {
            readThrowStrength = true;
            return Optional<float>{0.0f};
        });
        EXPECT_FALSE(readThrowStrength);
        EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
        EXPECT_FALSE(observation.hasRetainedThrowStrength);
    }
}

TEST(GrenadeThrowObservationTest, ReadsAndRetainsPinnedZeroStrengthForCurrentCycle)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle flashbang{4};
    bool readThrowStrength{};

    static_cast<void>(observation.observePinState(flashbang, true));
    observation.captureThrowStrength(true, {}, [&]() noexcept {
        readThrowStrength = true;
        return Optional<float>{0.0f};
    });

    EXPECT_TRUE(readThrowStrength);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.0f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadeThrowObservationParityTest, FinalizesLegacyReleaseWithoutRetainedPinPulledStrength)
{
    GrenadePredictionState state;
    constexpr cs2::CEntityHandle weapon{1};
    ASSERT_TRUE(state.throwObservation.observeWeapon(weapon));
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.tagTempTrajectory(weapon, state.throwObservation.pendingSequence());
    EXPECT_FALSE(state.throwObservation.observePinState(weapon, true));
    EXPECT_TRUE(state.completeLegacyHeldThrow(weapon, state.throwObservation.observePinState(weapon, false), true, 10.0f));
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

    observation.captureThrowStrength(false, Optional<float>{10.0f}, []() noexcept { return Optional<float>{0.25f}; });
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.25f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
    EXPECT_TRUE(observation.observeThrowTime(weapon, 10.0f));

    observation.captureThrowStrength(true, Optional<float>{10.0f}, []() noexcept { return Optional<float>{0.5f}; });
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.25f);
}

TEST(GrenadeThrowObservationParityTest, CapturesPinnedZeroStrength)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{1};
    static_cast<void>(observation.observeWeapon(weapon));

    observation.captureThrowStrength(true, {}, []() noexcept { return Optional<float>{0.0f}; });

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

    EXPECT_TRUE(state.completeHeldThrow(weapon, true, 10.1f));
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

    EXPECT_TRUE(state.completeHeldThrow(state.throwObservation.pendingWeapon(), true, 10.1f));
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

    EXPECT_TRUE(state.completeHeldThrow(state.throwObservation.pendingWeapon(), true, 10.1f));
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
    EXPECT_TRUE(state.throwObservation.observeWeapon(newWeapon));
}

TEST(GrenadePredictionAuthorityParityTest, PendingWeaponHandoffUsesStoredWeaponAndDeadline)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle oldWeapon{1};
    constexpr cs2::CEntityHandle newWeapon{2};
    ASSERT_TRUE(observation.observeWeapon(oldWeapon));
    ASSERT_TRUE(observation.observeThrowTime(oldWeapon, 5.0f));
    EXPECT_EQ(observation.pendingWeapon(), oldWeapon);
    EXPECT_TRUE(observation.consumeActualExecution(true, 6.0f));
    EXPECT_TRUE(observation.observeWeapon(newWeapon));
    EXPECT_FALSE(observation.hasPendingExecution());
}

}
