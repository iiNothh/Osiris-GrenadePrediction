#include <algorithm>
#include <array>
#include <limits>

#include <gtest/gtest.h>

#include <CS2/Classes/Vector.h>
#include <Features/Visuals/GrenadePrediction/Rendering/TrajectoryRenderPlan.h>

namespace
{

[[nodiscard]] float pointToSegmentDistanceSquared(const cs2::Vector& point, const cs2::Vector& first, const cs2::Vector& last) noexcept
{
    const float chordX = last.x - first.x;
    const float chordY = last.y - first.y;
    const float chordZ = last.z - first.z;
    const float chordLengthSquared = chordX * chordX + chordY * chordY + chordZ * chordZ;
    float projection = 0.0f;
    if (chordLengthSquared > 0.0f) {
        const float pointX = point.x - first.x;
        const float pointY = point.y - first.y;
        const float pointZ = point.z - first.z;
        projection = (pointX * chordX + pointY * chordY + pointZ * chordZ) / chordLengthSquared;
        if (projection < 0.0f)
            projection = 0.0f;
        else if (projection > 1.0f)
            projection = 1.0f;
    }
    const float errorX = point.x - (first.x + chordX * projection);
    const float errorY = point.y - (first.y + chordY * projection);
    const float errorZ = point.z - (first.z + chordZ * projection);
    return errorX * errorX + errorY * errorY + errorZ * errorZ;
}

[[nodiscard]] float maximumChordErrorSquared(const Trajectory& trajectory, const int* selectedPointIndices, int selectedPointCount) noexcept
{
    float maximumSquaredError = 0.0f;
    for (int segmentIndex = 1; segmentIndex < selectedPointCount; ++segmentIndex) {
        const int firstPointIndex = selectedPointIndices[segmentIndex - 1];
        const int lastPointIndex = selectedPointIndices[segmentIndex];
        for (int pointIndex = firstPointIndex + 1; pointIndex < lastPointIndex; ++pointIndex) {
            maximumSquaredError = std::max(maximumSquaredError,
                pointToSegmentDistanceSquared(trajectory.points[pointIndex], trajectory.points[firstPointIndex], trajectory.points[lastPointIndex]));
        }
    }
    return maximumSquaredError;
}

[[nodiscard]] bool hasSelectedPoint(const TrajectoryRenderPlan& plan, int targetPointIndex) noexcept
{
    for (int i = 0; i < plan.selectedPointCount; ++i) {
        if (plan.pointIndex(i) == targetPointIndex)
            return true;
    }
    return false;
}

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
    TrajectoryRenderPlan repeatedPlan;

    plan.build(trajectory);
    repeatedPlan.build(trajectory);

    EXPECT_EQ(plan.selectedPointCount, 159);
    EXPECT_EQ(plan.lineSegmentCount, 158);
    EXPECT_EQ(plan.pointIndex(0), 0);
    EXPECT_EQ(plan.pointIndex(plan.selectedPointCount - 1), Trajectory::kPointsCapacity - 1);
    for (int i = 0; i < plan.selectedPointCount; ++i) {
        EXPECT_EQ(plan.pointIndex(i), repeatedPlan.pointIndex(i));
        if (i > 0)
            EXPECT_LT(plan.pointIndex(i - 1), plan.pointIndex(i));
    }
    EXPECT_TRUE(hasSelectedPoint(plan, 250));
}

TEST(TrajectoryRenderPlanTest, SaturatesAvailableLinePanels)
{
    Trajectory trajectory;
    trajectory.pointsCount = Trajectory::kPointsCapacity;
    trajectory.validLanding = false;
    TrajectoryRenderPlan plan;

    plan.build(trajectory);

    EXPECT_EQ(plan.selectedPointCount, TrajectoryRenderPlan::kSelectedPointsCapacity - trajectory.markersCount);
    EXPECT_EQ(plan.lineSegmentCount, TrajectoryRenderPlan::kPanelCapacity);
    EXPECT_EQ(plan.pointIndex(0), 0);
    EXPECT_EQ(plan.pointIndex(plan.selectedPointCount - 1), Trajectory::kPointsCapacity - 1);
    for (int i = 1; i < plan.selectedPointCount; ++i)
        EXPECT_LT(plan.pointIndex(i - 1), plan.pointIndex(i));
}

TEST(TrajectoryRenderPlanTest, RetainsEveryPointWhenThePanelBudgetFits)
{
    Trajectory trajectory;
    trajectory.pointsCount = 40;
    trajectory.markersCount = 2;
    trajectory.markers[0] = {.pointIndex = 8};
    trajectory.markers[1] = {.pointIndex = 30};
    TrajectoryRenderPlan plan;

    plan.build(trajectory);

    EXPECT_EQ(plan.selectedPointCount, trajectory.pointsCount);
    EXPECT_EQ(plan.lineSegmentCount, trajectory.pointsCount - 1);
    for (int i = 0; i < trajectory.pointsCount; ++i)
        EXPECT_EQ(plan.pointIndex(i), i);
}

TEST(TrajectoryRenderPlanTest, PreservesCurvedRouteCornersWithLowerErrorThanUniformStride)
{
    Trajectory trajectory;
    trajectory.pointsCount = Trajectory::kPointsCapacity;
    trajectory.validLanding = false;
    constexpr std::array curvedY{0.0f, 10.0f, 35.0f, 70.0f, 100.0f, 70.0f, 35.0f, 10.0f, 0.0f};
    for (int i = 0; i < trajectory.pointsCount; ++i) {
        trajectory.points[i] = {.x = static_cast<float>(i), .y = 0.0f, .z = 0.0f};
        if (i < static_cast<int>(curvedY.size()))
            trajectory.points[i].y = curvedY[i];
    }
    TrajectoryRenderPlan plan;
    plan.build(trajectory);

    std::array<int, TrajectoryRenderPlan::kSelectedPointsCapacity> legacyStrideIndices{};
    for (int i = 0; i < TrajectoryRenderPlan::kSelectedPointsCapacity; ++i)
        legacyStrideIndices[i] = i * (trajectory.pointsCount - 1) / (TrajectoryRenderPlan::kSelectedPointsCapacity - 1);

    EXPECT_EQ(plan.selectedPointCount, TrajectoryRenderPlan::kSelectedPointsCapacity - trajectory.markersCount);
    for (int cornerIndex = 0; cornerIndex < static_cast<int>(curvedY.size()); ++cornerIndex)
        EXPECT_TRUE(hasSelectedPoint(plan, cornerIndex));
    EXPECT_LT(maximumChordErrorSquared(trajectory, plan.selectedPointIndices, plan.selectedPointCount),
        maximumChordErrorSquared(trajectory, legacyStrideIndices.data(), static_cast<int>(legacyStrideIndices.size())));
}

TEST(TrajectoryRenderPlanTest, SelectsDeterministicallyWithCoincidentAndDuplicatePositions)
{
    Trajectory trajectory;
    trajectory.pointsCount = Trajectory::kPointsCapacity;
    trajectory.markersCount = 3;
    trajectory.markers[0] = {.pointIndex = 100};
    trajectory.markers[1] = {.pointIndex = 100};
    trajectory.markers[2] = {.pointIndex = 200};
    trajectory.validLanding = false;
    for (int i = 0; i < trajectory.pointsCount; ++i)
        trajectory.points[i] = {.x = 4.0f, .y = -2.0f, .z = 1.0f};
    TrajectoryRenderPlan plan;
    TrajectoryRenderPlan repeatedPlan;

    plan.build(trajectory);
    repeatedPlan.build(trajectory);

    EXPECT_EQ(plan.selectedPointCount, TrajectoryRenderPlan::kSelectedPointsCapacity - trajectory.markersCount);
    for (int i = 0; i < plan.selectedPointCount; ++i) {
        EXPECT_EQ(plan.pointIndex(i), repeatedPlan.pointIndex(i));
        if (i > 0)
            EXPECT_LT(plan.pointIndex(i - 1), plan.pointIndex(i));
    }
    EXPECT_TRUE(hasSelectedPoint(plan, 100));
    EXPECT_TRUE(hasSelectedPoint(plan, 200));
}

TEST(TrajectoryRenderPlanTest, HandlesNonFiniteCoordinatesWithoutLosingMandatoryMarkers)
{
    Trajectory trajectory;
    trajectory.pointsCount = Trajectory::kPointsCapacity;
    trajectory.markersCount = 1;
    trajectory.markers[0] = {.pointIndex = 250};
    trajectory.validLanding = false;
    for (int i = 0; i < trajectory.pointsCount; ++i)
        trajectory.points[i] = {.x = static_cast<float>(i), .y = 0.0f, .z = 0.0f};
    trajectory.points[249].y = std::numeric_limits<float>::quiet_NaN();
    trajectory.points[250].x = std::numeric_limits<float>::infinity();
    TrajectoryRenderPlan plan;
    TrajectoryRenderPlan repeatedPlan;

    plan.build(trajectory);
    repeatedPlan.build(trajectory);

    EXPECT_EQ(plan.selectedPointCount, TrajectoryRenderPlan::kSelectedPointsCapacity - trajectory.markersCount);
    EXPECT_TRUE(hasSelectedPoint(plan, 250));
    for (int i = 0; i < plan.selectedPointCount; ++i) {
        EXPECT_EQ(plan.pointIndex(i), repeatedPlan.pointIndex(i));
        EXPECT_GE(plan.pointIndex(i), 0);
        EXPECT_LT(plan.pointIndex(i), trajectory.pointsCount);
        if (i > 0)
            EXPECT_LT(plan.pointIndex(i - 1), plan.pointIndex(i));
    }
}

}
