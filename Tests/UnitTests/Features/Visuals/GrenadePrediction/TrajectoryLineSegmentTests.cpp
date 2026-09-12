#include <limits>

#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Rendering/TrajectoryLineSegment.h>

namespace
{

TEST(TrajectoryLineSegmentTest, RejectsNonFiniteInput)
{
    TrajectoryLineSegment segment{};
    const auto nonFinite = std::numeric_limits<float>::quiet_NaN();

    EXPECT_FALSE(TrajectoryLineSegment::fromClipSpace({nonFinite, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 1.0f, segment));
}

TEST(TrajectoryLineSegmentTest, RejectsInvalidAspectRatio)
{
    TrajectoryLineSegment segment{};
    const ClipSpaceCoordinates first{-0.5f, 0.0f, 0.0f, 1.0f};
    const ClipSpaceCoordinates second{0.5f, 0.0f, 0.0f, 1.0f};

    EXPECT_FALSE(TrajectoryLineSegment::fromClipSpace(first, second, TrajectoryLineSegment::kNearW, segment));
    EXPECT_FALSE(TrajectoryLineSegment::fromClipSpace(first, second, std::numeric_limits<float>::quiet_NaN(), segment));
}

TEST(TrajectoryLineSegmentTest, RejectsBothEndpointsBehindNearPlane)
{
    TrajectoryLineSegment segment{};

    EXPECT_FALSE(TrajectoryLineSegment::fromClipSpace({-0.5f, 0.0f, 0.0f, 0.0f}, {0.5f, 0.0f, 0.0f, 0.0009f}, 1.0f, segment));
}

TEST(TrajectoryLineSegmentTest, ClipsFirstEndpointAtNearPlane)
{
    TrajectoryLineSegment segment{};

    ASSERT_TRUE(TrajectoryLineSegment::fromClipSpace({-0.0005f, 0.0f, 0.0f, 0.0f}, {0.5f, 0.0f, 0.0f, 1.0f}, 1.0f, segment));
    EXPECT_NEAR(segment.midpointX, 62.5125008f, 0.0001f);
    EXPECT_FLOAT_EQ(segment.midpointY, 50.0f);
    EXPECT_NEAR(segment.width, 24.9750023f, 0.0001f);
    EXPECT_FLOAT_EQ(segment.angleDegrees, 0.0f);
}

TEST(TrajectoryLineSegmentTest, ClipsSecondEndpointAtNearPlane)
{
    TrajectoryLineSegment segment{};

    ASSERT_TRUE(TrajectoryLineSegment::fromClipSpace({0.5f, 0.0f, 0.0f, 1.0f}, {-0.0005f, 0.0f, 0.0f, 0.0f}, 1.0f, segment));
    EXPECT_NEAR(segment.midpointX, 62.5119209f, 0.0001f);
    EXPECT_FLOAT_EQ(segment.midpointY, 50.0f);
    EXPECT_NEAR(segment.width, 24.9761581f, 0.0001f);
    EXPECT_NEAR(segment.angleDegrees, 180.0f, 0.001f);
}

TEST(TrajectoryLineSegmentTest, ClipsSegmentToViewport)
{
    TrajectoryLineSegment segment{};

    ASSERT_TRUE(TrajectoryLineSegment::fromClipSpace({-2.0f, 0.0f, 0.0f, 1.0f}, {2.0f, 0.0f, 0.0f, 1.0f}, 1.0f, segment));
    EXPECT_FLOAT_EQ(segment.midpointX, 50.0f);
    EXPECT_FLOAT_EQ(segment.midpointY, 50.0f);
    EXPECT_FLOAT_EQ(segment.width, 100.0f);
    EXPECT_FLOAT_EQ(segment.angleDegrees, 0.0f);
}

TEST(TrajectoryLineSegmentTest, RejectsDegenerateSegment)
{
    TrajectoryLineSegment segment{};
    const ClipSpaceCoordinates point{0.0f, 0.0f, 0.0f, 1.0f};

    EXPECT_FALSE(TrajectoryLineSegment::fromClipSpace(point, point, 1.0f, segment));
}

TEST(TrajectoryLineSegmentTest, CalculatesExactMetricsForRepresentativeSegments)
{
    TrajectoryLineSegment horizontalSegment{};
    TrajectoryLineSegment verticalSegment{};

    ASSERT_TRUE(TrajectoryLineSegment::fromClipSpace({-0.5f, 0.0f, 0.0f, 1.0f}, {0.5f, 0.0f, 0.0f, 1.0f}, 1.0f, horizontalSegment));
    EXPECT_FLOAT_EQ(horizontalSegment.midpointX, 50.0f);
    EXPECT_FLOAT_EQ(horizontalSegment.midpointY, 50.0f);
    EXPECT_FLOAT_EQ(horizontalSegment.width, 50.0f);
    EXPECT_FLOAT_EQ(horizontalSegment.angleDegrees, 0.0f);

    ASSERT_TRUE(TrajectoryLineSegment::fromClipSpace({0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f, 1.0f}, 1.0f, verticalSegment));
    EXPECT_FLOAT_EQ(verticalSegment.midpointX, 50.0f);
    EXPECT_FLOAT_EQ(verticalSegment.midpointY, 50.0f);
    EXPECT_FLOAT_EQ(verticalSegment.width, 100.0f);
    EXPECT_FLOAT_EQ(verticalSegment.angleDegrees, 90.0f);
}

}
