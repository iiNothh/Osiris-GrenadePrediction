#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Rendering/GrenadeTrajectoryLine.h>

namespace
{

struct ClipPoint {
    float x;
    float y;
    float z;
    float w;
};

TEST(GrenadeTrajectoryLineTests, ClipsSegmentToViewport)
{
    grenade_prediction::GrenadeTrajectoryLine line{};
    const auto projected = grenade_prediction::GrenadeTrajectoryLineClipper::project(cs2::Vector{-2.0f, 0.0f, 0.0f}, cs2::Vector{2.0f, 0.0f, 0.0f}, [](const cs2::Vector& point) {
        return ClipPoint{point.x, point.y, point.z, 1.0f};
    }, line);

    ASSERT_TRUE(projected);
    EXPECT_FLOAT_EQ(line.start.x, -1.0f);
    EXPECT_FLOAT_EQ(line.end.x, 1.0f);
    EXPECT_FLOAT_EQ(line.start.y, 0.0f);
    EXPECT_FLOAT_EQ(line.end.y, 0.0f);
}

TEST(GrenadeTrajectoryLineTests, RejectsSegmentOutsideViewport)
{
    grenade_prediction::GrenadeTrajectoryLine line{};
    EXPECT_FALSE(grenade_prediction::GrenadeTrajectoryLineClipper::project(cs2::Vector{-3.0f, 2.0f, 0.0f}, cs2::Vector{-2.0f, 3.0f, 0.0f}, [](const cs2::Vector& point) {
        return ClipPoint{point.x, point.y, point.z, 1.0f};
    }, line));
}

TEST(GrenadeTrajectoryLineTests, CalculatesLengthAndAngle)
{
    const grenade_prediction::GrenadeTrajectoryLine line{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
    EXPECT_NEAR(line.length(), 1.4142135f, 0.0001f);
    EXPECT_NEAR(line.angleDegrees(), 45.0f, 0.0001f);
}

}
