#include <limits>

#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/TrajectoryLineSegment.h>

TEST(TrajectoryLineSegmentTest, BuildsHorizontalGeometry)
{
    TrajectoryLineSegment segment{};
    EXPECT_TRUE(TrajectoryLineSegment::fromClipSpace({-.5f, 0.0f, 0.0f, 1.0f}, {.5f, 0.0f, 0.0f, 1.0f}, 1.0f, segment));
    EXPECT_FLOAT_EQ(segment.midpointX, 50.0f);
    EXPECT_FLOAT_EQ(segment.midpointY, 50.0f);
    EXPECT_FLOAT_EQ(segment.width, 50.0f);
    EXPECT_FLOAT_EQ(segment.angleDegrees, 0.0f);
}

TEST(TrajectoryLineSegmentTest, UsesAspectCorrectVerticalGeometry)
{
    TrajectoryLineSegment segment{};
    EXPECT_TRUE(TrajectoryLineSegment::fromClipSpace({0.0f, .5f, 0.0f, 1.0f}, {0.0f, -.5f, 0.0f, 1.0f}, 2.0f, segment));
    EXPECT_FLOAT_EQ(segment.midpointX, 50.0f);
    EXPECT_FLOAT_EQ(segment.midpointY, 50.0f);
    EXPECT_FLOAT_EQ(segment.width, 25.0f);
    EXPECT_FLOAT_EQ(segment.angleDegrees, 90.0f);
}

TEST(TrajectoryLineSegmentTest, BuildsAccurateNonAxisAngle)
{
    TrajectoryLineSegment segment{};
    EXPECT_TRUE(TrajectoryLineSegment::fromClipSpace({0.0f, 0.0f, 0.0f, 1.0f}, {.5f, -.25f, 0.0f, 1.0f}, 1.0f, segment));
    EXPECT_NEAR(segment.angleDegrees, 26.565051f, 0.01f);
}

TEST(TrajectoryLineSegmentTest, ClipsToViewport)
{
    TrajectoryLineSegment segment{};
    EXPECT_TRUE(TrajectoryLineSegment::fromClipSpace({-2.0f, 0.0f, 0.0f, 1.0f}, {2.0f, 0.0f, 0.0f, 1.0f}, 1.0f, segment));
    EXPECT_FLOAT_EQ(segment.midpointX, 50.0f);
    EXPECT_FLOAT_EQ(segment.width, 100.0f);
}

TEST(TrajectoryLineSegmentTest, ClipsNearPlane)
{
    TrajectoryLineSegment segment{};
    EXPECT_TRUE(TrajectoryLineSegment::fromClipSpace({-.0005f, 0.0f, 0.0f, 0.0f}, {.5f, 0.0f, 0.0f, 1.0f}, 1.0f, segment));
}

TEST(TrajectoryLineSegmentTest, RejectsBothBehindNearPlane)
{
    TrajectoryLineSegment segment{};
    EXPECT_FALSE(TrajectoryLineSegment::fromClipSpace({0.0f, 0.0f, 0.0f, 0.0f}, {.5f, 0.0f, 0.0f, -.1f}, 1.0f, segment));
}

TEST(TrajectoryLineSegmentTest, RejectsNonFiniteInputs)
{
    TrajectoryLineSegment segment{};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(TrajectoryLineSegment::fromClipSpace({nan, 0.0f, 0.0f, 1.0f}, {.5f, 0.0f, 0.0f, 1.0f}, 1.0f, segment));
}
