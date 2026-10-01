#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadePredictionConfigVariables.h>
#include <Config/ConfigVariableTypes.h>
#include <Config/ConfigVariables.h>

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
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(0.004f), 0.0f);
    EXPECT_FLOAT_EQ(grenade_prediction_vars::normalizeCacheDuration(0.0051f), 0.01f);
    EXPECT_FLOAT_EQ(grenade_prediction_vars::normalizeCacheDuration(1.234f), 1.23f);
    EXPECT_FLOAT_EQ(grenade_prediction_vars::normalizeCacheDuration(1.235f), 1.24f);
    EXPECT_FLOAT_EQ(grenade_prediction_vars::normalizeCacheDuration(59.994f), 59.99f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(59.996f), 60.0f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(61.0f), 60.0f);
}

TEST(GrenadePredictionConfigVariablesTest, NormalizesCacheDurationToSliderIncrement)
{
    EXPECT_FLOAT_EQ(grenade_prediction_vars::normalizeCacheDuration(1.534f), 1.53f);
    EXPECT_FLOAT_EQ(grenade_prediction_vars::normalizeCacheDuration(-0.006f), 0.0f);
    EXPECT_FLOAT_EQ(grenade_prediction_vars::normalizeCacheDuration(60.006f), 60.0f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(1.5f), 1.5f);
}

TEST(GrenadePredictionConfigVariablesTest, ClampsNonFiniteCacheDuration)
{
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(std::numeric_limits<float>::quiet_NaN()), 0.0f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(-std::numeric_limits<float>::infinity()), 0.0f);
    EXPECT_EQ(grenade_prediction_vars::normalizeCacheDuration(std::numeric_limits<float>::infinity()), 60.0f);
}

TEST(GrenadePredictionConfigVariablesTest, TrajectoryThicknessDefaultsToExistingTwoPixelAppearance)
{
    using namespace grenade_prediction_vars;
    static_assert(TrajectoryThickness::ValueType::kMin == 0.5f);
    static_assert(TrajectoryThickness::ValueType::kMax == 3.0f);
    static_assert(static_cast<float>(TrajectoryThickness::kDefaultValue) == 2.0f);
    static_assert(kTrajectoryThicknessSliderScale == 100);
    static_assert(kTrajectoryThicknessSliderIncrement == 0.01f);
    static_assert(kTrajectoryThicknessSliderDecimalPlaces == 2U);

    ConfigVariables variables;
    EXPECT_FLOAT_EQ(static_cast<float>(variables.getVariableValue<TrajectoryThickness>()), 2.0f);
    EXPECT_FLOAT_EQ(normalizeTrajectoryThickness(TrajectoryThickness::kDefaultValue), 2.0f);
    int registrations = 0;
    ConfigVariableTypes::forEach([&]<typename ConfigVariable>(std::type_identity<ConfigVariable>) {
        if constexpr (std::is_same_v<ConfigVariable, TrajectoryThickness>)
            ++registrations;
    });
    EXPECT_EQ(registrations, 1);
}

struct TrajectoryThicknessNormalizationCase {
    float input;
    float expected;
};

class GrenadeTrajectoryThicknessNormalizationTest : public testing::TestWithParam<TrajectoryThicknessNormalizationCase> {};

TEST_P(GrenadeTrajectoryThicknessNormalizationTest, ClampsAndSnapsToTwoDecimalPlaces)
{
    const auto [input, expected] = GetParam();
    const auto normalized = grenade_prediction_vars::normalizeTrajectoryThickness(input);
    EXPECT_FLOAT_EQ(normalized, expected);
    EXPECT_FLOAT_EQ(grenade_prediction_vars::normalizeTrajectoryThickness(normalized), normalized);
}

INSTANTIATE_TEST_SUITE_P(BoundariesAndNonFiniteValues, GrenadeTrajectoryThicknessNormalizationTest, testing::Values(
    TrajectoryThicknessNormalizationCase{-1.0f, 0.5f},
    TrajectoryThicknessNormalizationCase{0.0f, 0.5f},
    TrajectoryThicknessNormalizationCase{0.004f, 0.5f},
    TrajectoryThicknessNormalizationCase{0.5f, 0.5f},
    TrajectoryThicknessNormalizationCase{0.504f, 0.5f},
    TrajectoryThicknessNormalizationCase{0.5051f, 0.51f},
    TrajectoryThicknessNormalizationCase{1.234f, 1.23f},
    TrajectoryThicknessNormalizationCase{1.235f, 1.24f},
    TrajectoryThicknessNormalizationCase{1.23f, 1.23f},
    TrajectoryThicknessNormalizationCase{2.0f, 2.0f},
    TrajectoryThicknessNormalizationCase{2.994f, 2.99f},
    TrajectoryThicknessNormalizationCase{2.996f, 3.0f},
    TrajectoryThicknessNormalizationCase{3.0f, 3.0f},
    TrajectoryThicknessNormalizationCase{4.0f, 3.0f},
    TrajectoryThicknessNormalizationCase{std::numeric_limits<float>::max(), 3.0f},
    TrajectoryThicknessNormalizationCase{std::numeric_limits<float>::quiet_NaN(), 0.5f},
    TrajectoryThicknessNormalizationCase{-std::numeric_limits<float>::infinity(), 0.5f},
    TrajectoryThicknessNormalizationCase{std::numeric_limits<float>::infinity(), 3.0f}));

}
