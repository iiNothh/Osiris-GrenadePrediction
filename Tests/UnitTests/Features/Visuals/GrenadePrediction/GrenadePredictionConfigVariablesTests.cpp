#include <limits>

#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadePredictionConfigVariables.h>

namespace
{

TEST(GrenadePredictionConfigVariablesTest, NormalizesEveryVisibilityModeAndInvalidValues)
{
    EXPECT_EQ(grenade_prediction_vars::normalizeLastTrajectoryVisibilityMode(0), grenade_prediction_vars::LastTrajectoryVisibilityMode::Explode);
    EXPECT_EQ(grenade_prediction_vars::normalizeLastTrajectoryVisibilityMode(1), grenade_prediction_vars::LastTrajectoryVisibilityMode::Always);
    EXPECT_EQ(grenade_prediction_vars::normalizeLastTrajectoryVisibilityMode(2), grenade_prediction_vars::LastTrajectoryVisibilityMode::Off);
    EXPECT_EQ(grenade_prediction_vars::normalizeLastTrajectoryVisibilityMode(3), grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom);
    EXPECT_EQ(grenade_prediction_vars::normalizeLastTrajectoryVisibilityMode(4), grenade_prediction_vars::LastTrajectoryVisibilityMode::Explode);
    EXPECT_EQ(grenade_prediction_vars::normalizeLastTrajectoryVisibilityMode(255), grenade_prediction_vars::LastTrajectoryVisibilityMode::Explode);
}

TEST(GrenadePredictionConfigVariablesTest, ClampsAndRoundsCacheDuration)
{
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(-1.0f), 0.0f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(0.04f), 0.0f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(0.05f), 0.1f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(1.24f), 1.2f);
    EXPECT_FLOAT_EQ(grenade_prediction_vars::normalizeCacheDuration(1.25f), 1.3f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(59.94f), 59.9f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(59.95f), 60.0f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(61.0f), 60.0f);
}

TEST(GrenadePredictionConfigVariablesTest, ClampsNonFiniteCacheDuration)
{
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(std::numeric_limits<float>::quiet_NaN()), 0.0f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(-std::numeric_limits<float>::infinity()), 0.0f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(std::numeric_limits<float>::infinity()), 60.0f);
}

}
