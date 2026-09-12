#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Trajectory.h>

namespace
{

TEST(TrajectoryTest, CopyFromCopiesActivePayloadWithoutReplacingInactiveTails)
{
    Trajectory source;
    source.pointsCount = 2;
    source.points[0] = {1.0f, 2.0f, 3.0f};
    source.points[1] = {4.0f, 5.0f, 6.0f};
    source.points[2] = {7.0f, 8.0f, 9.0f};
    source.markersCount = 1;
    source.worldContactMarkersCount = 1;
    source.markers[0] = {1};
    source.markers[1] = {2};
    source.endPos = {10.0f, 11.0f, 12.0f};
    source.valid = true;
    source.validLanding = false;
    Trajectory destination;
    destination.points[2] = {13.0f, 14.0f, 15.0f};
    destination.markers[1] = {3};

    destination.copyFrom(source);

    EXPECT_EQ(destination.pointsCount, source.pointsCount);
    EXPECT_EQ(destination.points[0], source.points[0]);
    EXPECT_EQ(destination.points[1], source.points[1]);
    EXPECT_EQ(destination.points[2], (cs2::Vector{13.0f, 14.0f, 15.0f}));
    EXPECT_EQ(destination.markersCount, source.markersCount);
    EXPECT_EQ(destination.worldContactMarkersCount, source.worldContactMarkersCount);
    EXPECT_EQ(destination.markers[0].pointIndex, source.markers[0].pointIndex);
    EXPECT_EQ(destination.markers[1].pointIndex, 3);
    EXPECT_EQ(destination.endPos, source.endPos);
    EXPECT_EQ(destination.valid, source.valid);
    EXPECT_EQ(destination.validLanding, source.validLanding);
}

}
