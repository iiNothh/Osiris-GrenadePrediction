#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Rendering/TrajectoryRenderPlan.h>

namespace
{

TEST(TrajectoryRenderPlanTest, RejectsInvalidCounts)
{
    Trajectory trajectory;
    TrajectoryRenderPlan plan;

    trajectory.pointsCount = 1;
    plan.build(trajectory);
    EXPECT_EQ(plan.selectedPointCount, 0);
    EXPECT_EQ(plan.lineSegmentCount, 0);

    trajectory.pointsCount = Trajectory::kPointsCapacity + 1;
    plan.build(trajectory);
    EXPECT_EQ(plan.selectedPointCount, 0);
    EXPECT_EQ(plan.lineSegmentCount, 0);

    trajectory.pointsCount = 2;
    trajectory.markersCount = -1;
    plan.build(trajectory);
    EXPECT_EQ(plan.selectedPointCount, 0);
    EXPECT_EQ(plan.lineSegmentCount, 0);

    trajectory.markersCount = Trajectory::kMarkersCapacity + 1;
    plan.build(trajectory);
    EXPECT_EQ(plan.selectedPointCount, 0);
    EXPECT_EQ(plan.lineSegmentCount, 0);
}

TEST(TrajectoryRenderPlanTest, SelectsMandatoryEndpointsAndMarkers)
{
    Trajectory trajectory;
    trajectory.pointsCount = Trajectory::kPointsCapacity;
    trajectory.markersCount = 3;
    trajectory.markers[0] = {.pointIndex = 20};
    trajectory.markers[1] = {.pointIndex = 250};
    trajectory.markers[2] = {.pointIndex = 480};
    trajectory.validLanding = false;
    TrajectoryRenderPlan plan;

    plan.build(trajectory);

    EXPECT_EQ(plan.pointIndex(0), 0);
    EXPECT_EQ(plan.pointIndex(plan.selectedPointCount - 1), Trajectory::kPointsCapacity - 1);
    bool hasFirstMarker = false;
    bool hasSecondMarker = false;
    bool hasThirdMarker = false;
    for (int i = 0; i < plan.selectedPointCount; ++i) {
        hasFirstMarker = hasFirstMarker || plan.pointIndex(i) == trajectory.markers[0].pointIndex;
        hasSecondMarker = hasSecondMarker || plan.pointIndex(i) == trajectory.markers[1].pointIndex;
        hasThirdMarker = hasThirdMarker || plan.pointIndex(i) == trajectory.markers[2].pointIndex;
    }
    EXPECT_TRUE(hasFirstMarker);
    EXPECT_TRUE(hasSecondMarker);
    EXPECT_TRUE(hasThirdMarker);
}

TEST(TrajectoryRenderPlanTest, DeduplicatesMandatoryPoints)
{
    Trajectory trajectory;
    trajectory.pointsCount = 4;
    trajectory.markersCount = 4;
    trajectory.markers[0] = {.pointIndex = 0};
    trajectory.markers[1] = {.pointIndex = 3};
    trajectory.markers[2] = {.pointIndex = 1};
    trajectory.markers[3] = {.pointIndex = 1};
    trajectory.validLanding = false;
    TrajectoryRenderPlan plan;

    plan.build(trajectory);

    EXPECT_EQ(plan.selectedPointCount, 4);
    EXPECT_EQ(plan.lineSegmentCount, 3);
    for (int i = 0; i < plan.selectedPointCount; ++i)
        EXPECT_EQ(plan.pointIndex(i), i);
}

TEST(TrajectoryRenderPlanTest, ReservesPanelsForMarkersAndLanding)
{
    Trajectory trajectory;
    trajectory.pointsCount = Trajectory::kPointsCapacity;
    trajectory.markersCount = Trajectory::kMarkersCapacity;
    for (int i = 0; i < trajectory.markersCount; ++i)
        trajectory.markers[i] = {.pointIndex = (i + 1) * 20};
    trajectory.validLanding = true;
    TrajectoryRenderPlan plan;

    plan.build(trajectory);

    EXPECT_EQ(plan.selectedPointCount, 139);
    EXPECT_EQ(plan.lineSegmentCount, 138);
    EXPECT_EQ(plan.lineSegmentCount + trajectory.markersCount + 1, TrajectoryRenderPlan::kPanelCapacity);
}

TEST(TrajectoryRenderPlanTest, AllocatesGapPointsDeterministically)
{
    Trajectory trajectory;
    trajectory.pointsCount = Trajectory::kPointsCapacity;
    trajectory.markersCount = 1;
    trajectory.markers[0] = {.pointIndex = 250};
    trajectory.validLanding = true;
    TrajectoryRenderPlan plan;

    plan.build(trajectory);

    EXPECT_EQ(plan.selectedPointCount, 159);
    EXPECT_EQ(plan.lineSegmentCount, 158);
    EXPECT_EQ(plan.pointIndex(0), 0);
    EXPECT_EQ(plan.pointIndex(1), 3);
    EXPECT_EQ(plan.pointIndex(78), 246);
    EXPECT_EQ(plan.pointIndex(79), 250);
    EXPECT_EQ(plan.pointIndex(80), 253);
    EXPECT_EQ(plan.pointIndex(157), 495);
    EXPECT_EQ(plan.pointIndex(158), 499);
}

TEST(TrajectoryRenderPlanTest, SaturatesAvailableLinePanels)
{
    Trajectory trajectory;
    trajectory.pointsCount = Trajectory::kPointsCapacity;
    trajectory.validLanding = false;
    TrajectoryRenderPlan plan;

    plan.build(trajectory);

    EXPECT_EQ(plan.selectedPointCount, TrajectoryRenderPlan::kSelectedPointsCapacity);
    EXPECT_EQ(plan.lineSegmentCount, TrajectoryRenderPlan::kPanelCapacity);
    EXPECT_EQ(plan.pointIndex(0), 0);
    EXPECT_EQ(plan.pointIndex(1), 3);
    EXPECT_EQ(plan.pointIndex(plan.selectedPointCount - 2), 495);
    EXPECT_EQ(plan.pointIndex(plan.selectedPointCount - 1), Trajectory::kPointsCapacity - 1);
}

}
