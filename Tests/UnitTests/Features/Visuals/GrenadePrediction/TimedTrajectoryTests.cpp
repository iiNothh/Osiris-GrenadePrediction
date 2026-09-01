#include <limits>

#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Live/TimedTrajectory.h>

namespace
{

void appendPoint(Trajectory& trajectory, cs2::Vector point, float elapsedTime)
{
    ASSERT_TRUE(trajectory.appendPoint(point, elapsedTime));
}

Trajectory makeTrajectory()
{
    Trajectory trajectory;
    static_cast<void>(trajectory.appendPoint({0.0f, 0.0f, 0.0f}, 0.0f));
    static_cast<void>(trajectory.appendPoint({10.0f, 0.0f, 0.0f}, 1.0f));
    static_cast<void>(trajectory.appendPoint({20.0f, 0.0f, 0.0f}, 2.0f));
    trajectory.endPos = {20.0f, 0.0f, 0.0f};
    trajectory.valid = true;
    return trajectory;
}

TEST(TimedTrajectoryTest, SamplesExactAndInterpolatedPositions)
{
    const auto trajectory = makeTrajectory();

    const auto exact = timed_trajectory::samplePosition(trajectory, 1.0f);
    const auto interpolated = timed_trajectory::samplePosition(trajectory, 1.5f);
    ASSERT_TRUE(exact.hasValue());
    ASSERT_TRUE(interpolated.hasValue());
    EXPECT_EQ(exact.value(), (cs2::Vector{10.0f, 0.0f, 0.0f}));
    EXPECT_EQ(interpolated.value(), (cs2::Vector{15.0f, 0.0f, 0.0f}));
}

TEST(TimedTrajectoryTest, RejectsInvalidTimeMetadataAndOutOfRangeSamples)
{
    auto trajectory = makeTrajectory();
    EXPECT_FALSE(timed_trajectory::samplePosition(trajectory, -0.1f).hasValue());
    EXPECT_FALSE(timed_trajectory::samplePosition(trajectory, 2.1f).hasValue());
    EXPECT_FALSE(timed_trajectory::samplePosition(trajectory, std::numeric_limits<float>::quiet_NaN()).hasValue());

    trajectory.elapsedTimes[2] = trajectory.elapsedTimes[1];
    EXPECT_FALSE(timed_trajectory::samplePosition(trajectory, 1.0f).hasValue());
    trajectory.elapsedTimes[2] = 2.0f;
    trajectory.points[1].x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(timed_trajectory::samplePosition(trajectory, 1.0f).hasValue());
}

TEST(TimedTrajectoryTest, RecordsExplicitMarkerKinds)
{
    Trajectory trajectory;
    appendPoint(trajectory, {}, 0.0f);
    ASSERT_TRUE(trajectory.appendWorldContactMarker());
    appendPoint(trajectory, {1.0f, 0.0f, 0.0f}, 1.0f);
    ASSERT_TRUE(trajectory.appendPlayerResponseMarker());

    ASSERT_EQ(trajectory.markersCount, 2);
    EXPECT_EQ(trajectory.markers[0].kind, TrajectoryMarkerKind::WorldContact);
    EXPECT_EQ(trajectory.markers[1].kind, TrajectoryMarkerKind::PlayerResponse);
}

TEST(TimedTrajectoryTest, ReplacesFutureWithActualJoinAndRemappedCandidateMarkers)
{
    auto published = makeTrajectory();
    ASSERT_TRUE(published.appendWorldContactMarker());
    published.markers[0].pointIndex = 0;
    ASSERT_TRUE(published.appendPlayerResponseMarker());

    auto candidate = makeTrajectory();
    candidate.points[0] = {100.0f, 0.0f, 0.0f};
    candidate.points[1] = {110.0f, 0.0f, 0.0f};
    candidate.points[2] = {120.0f, 0.0f, 0.0f};
    candidate.endPos = candidate.points[2];
    ASSERT_TRUE(candidate.appendWorldContactMarker());
    candidate.markers[0].pointIndex = 1;
    ASSERT_TRUE(candidate.appendPlayerResponseMarker());
    ASSERT_TRUE(candidate.appendPlayerResponseMarker());
    candidate.markers[2].pointIndex = 0;

    ASSERT_TRUE(timed_trajectory::replaceFuture(published, candidate, {.publishedPointIndex = 1, .candidatePointIndex = 0,
        .position = {7.0f, 0.0f, 0.0f}, .elapsedTime = 0.75f}));

    ASSERT_EQ(published.pointsCount, 4);
    EXPECT_EQ(published.points[0], (cs2::Vector{0.0f, 0.0f, 0.0f}));
    EXPECT_EQ(published.points[1], (cs2::Vector{7.0f, 0.0f, 0.0f}));
    EXPECT_EQ(published.points[2], candidate.points[1]);
    EXPECT_EQ(published.points[3], candidate.points[2]);
    EXPECT_NE(published.points[1], candidate.points[0]);
    EXPECT_FLOAT_EQ(published.elapsedTimes[2], 1.75f);
    EXPECT_EQ(published.markersCount, 3);
    EXPECT_EQ(published.markers[0].pointIndex, 0);
    EXPECT_EQ(published.markers[0].kind, TrajectoryMarkerKind::WorldContact);
    EXPECT_EQ(published.markers[1].pointIndex, 2);
    EXPECT_EQ(published.markers[1].kind, TrajectoryMarkerKind::WorldContact);
    EXPECT_EQ(published.markers[2].pointIndex, 3);
    EXPECT_EQ(published.markers[2].kind, TrajectoryMarkerKind::PlayerResponse);
    EXPECT_EQ(published.worldContactMarkersCount, 2);
    EXPECT_EQ(published.endPos, candidate.endPos);
}

TEST(TimedTrajectoryTest, DoesNotMutateDestinationWhenMergePreflightFails)
{
    auto published = makeTrajectory();
    const auto original = published;
    auto candidate = makeTrajectory();
    candidate.elapsedTimes[2] = candidate.elapsedTimes[1];

    EXPECT_FALSE(timed_trajectory::replaceFuture(published, candidate, {.publishedPointIndex = 1, .candidatePointIndex = 0,
        .position = {7.0f, 0.0f, 0.0f}, .elapsedTime = 0.75f}));
    EXPECT_EQ(published.pointsCount, original.pointsCount);
    EXPECT_EQ(published.points[0], original.points[0]);
    EXPECT_EQ(published.points[1], original.points[1]);
    EXPECT_EQ(published.elapsedTimes[1], original.elapsedTimes[1]);
    EXPECT_EQ(published.markersCount, original.markersCount);
    EXPECT_EQ(published.endPos, original.endPos);
}

TEST(TimedTrajectoryTest, DoesNotMutateDestinationWhenMergeWouldExceedPointCapacity)
{
    auto published = makeTrajectory();
    const auto original = published;
    auto candidate = makeTrajectory();
    for (int i = candidate.pointsCount; i < Trajectory::kPointsCapacity; ++i)
        ASSERT_TRUE(candidate.appendPoint({static_cast<float>(i), 0.0f, 0.0f}, static_cast<float>(i)));
    candidate.endPos = candidate.points[candidate.pointsCount - 1];

    EXPECT_FALSE(timed_trajectory::replaceFuture(published, candidate, {.publishedPointIndex = 1, .candidatePointIndex = 0,
        .position = {7.0f, 0.0f, 0.0f}, .elapsedTime = 0.75f}));
    EXPECT_EQ(published.pointsCount, original.pointsCount);
    EXPECT_EQ(published.points[1], original.points[1]);
    EXPECT_EQ(published.elapsedTimes[1], original.elapsedTimes[1]);
    EXPECT_EQ(published.endPos, original.endPos);
}

}
