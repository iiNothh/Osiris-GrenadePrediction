#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadePredictionConfigVariables.h>

TEST(GrenadePredictionConfig, VisibilityModeAndDurationAreNormalizedForUi)
{
    EXPECT_EQ(grenade_prediction_vars::normalizeLastTrajectoryVisibilityMode(0), grenade_prediction_vars::LastTrajectoryVisibilityMode::Explode);
    EXPECT_EQ(grenade_prediction_vars::normalizeLastTrajectoryVisibilityMode(255), grenade_prediction_vars::LastTrajectoryVisibilityMode::Explode);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(-1.0f), 0.0f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(61.0f), 60.0f);
}
