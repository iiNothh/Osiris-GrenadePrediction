#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Held/GrenadePredictionUpdateScheduler.h>

namespace
{

TEST(GrenadePredictionUpdateSchedulerTest, AdvancesSchedulerBeforeAWeaponDependentExit)
{
    GrenadePredictionUpdateScheduler scheduler;

    EXPECT_TRUE(scheduler.shouldUpdate(false, true, 1.0f / 240.0f));
    EXPECT_TRUE(scheduler.initialized);
    EXPECT_FALSE(scheduler.shouldUpdate(false, true, 1.0f / 240.0f));
}

TEST(GrenadePredictionUpdateSchedulerTest, TreatsNonFiniteFrametimeAsUnavailable)
{
    GrenadePredictionUpdateScheduler scheduler;

    EXPECT_TRUE(scheduler.shouldUpdate(false, true, 1.0f / 240.0f));
    scheduler.accumulatedTime = GrenadePredictionUpdateScheduler::updateInterval * 0.5f;
    EXPECT_TRUE(scheduler.shouldUpdate(false, false, 0.0f));
    EXPECT_FLOAT_EQ(scheduler.accumulatedTime, 0.0f);
}

}
