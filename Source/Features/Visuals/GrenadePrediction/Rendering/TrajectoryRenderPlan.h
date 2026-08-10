#pragma once

#include <Features/Visuals/GrenadePrediction/Trajectory.h>

struct TrajectoryRenderPlan {
    static constexpr int kPanelCapacity = 160;
    static constexpr int kSelectedPointsCapacity = kPanelCapacity + 1;

    int selectedPointIndices[kSelectedPointsCapacity]{};
    int selectedPointCount{};
    int lineSegmentCount{};

    void build(const Trajectory& trajectory) noexcept
    {
        selectedPointCount = 0;
        lineSegmentCount = 0;

        if (trajectory.pointsCount < 2 || trajectory.pointsCount > Trajectory::kPointsCapacity
            || trajectory.markersCount < 0 || trajectory.markersCount > Trajectory::kMarkersCapacity)
            return;

        addMandatoryPoint(0);
        addMandatoryPoint(trajectory.pointsCount - 1);
        for (int i = 0; i < trajectory.markersCount; ++i) {
            const int pointIndex = trajectory.markers[i].pointIndex;
            if (pointIndex >= 0 && pointIndex < trajectory.pointsCount)
                addMandatoryPoint(pointIndex);
        }

        const int markerPanelCount = trajectory.markersCount + (trajectory.validLanding ? 1 : 0);
        const int linePanelCapacity = kPanelCapacity - markerPanelCount;
        const int maximumSelectedPointCount = linePanelCapacity + 1;
        if (selectedPointCount >= maximumSelectedPointCount) {
            lineSegmentCount = selectedPointCount - 1;
            return;
        }

        int totalGaps = 0;
        for (int i = 1; i < selectedPointCount; ++i)
            totalGaps += selectedPointIndices[i] - selectedPointIndices[i - 1] - 1;

        const int pointsToAdd = totalGaps < maximumSelectedPointCount - selectedPointCount
            ? totalGaps
            : maximumSelectedPointCount - selectedPointCount;
        int previousGapEnd = 0;
        int previousAllocation = 0;
        int originalMandatoryCount = selectedPointCount;
        for (int i = 1; i < originalMandatoryCount; ++i) {
            const int gap = selectedPointIndices[i] - selectedPointIndices[i - 1] - 1;
            const int gapEnd = previousGapEnd + gap;
            const int allocation = totalGaps > 0 ? gapEnd * pointsToAdd / totalGaps : 0;
            const int intervalPointCount = allocation - previousAllocation;
            for (int point = 0; point < intervalPointCount; ++point) {
                const int first = selectedPointIndices[i - 1];
                const int pointIndex = first + (point + 1) * (gap + 1) / (intervalPointCount + 1);
                selectedPointIndices[selectedPointCount++] = pointIndex;
            }
            previousGapEnd = gapEnd;
            previousAllocation = allocation;
        }

        sortSelectedPoints();
        lineSegmentCount = selectedPointCount - 1;
    }

    [[nodiscard]] int pointIndex(int index) const noexcept
    {
        return selectedPointIndices[index];
    }

private:
    void addMandatoryPoint(int pointIndex) noexcept
    {
        int insertionIndex = 0;
        while (insertionIndex < selectedPointCount && selectedPointIndices[insertionIndex] < pointIndex)
            ++insertionIndex;
        if (insertionIndex < selectedPointCount && selectedPointIndices[insertionIndex] == pointIndex)
            return;
        for (int i = selectedPointCount; i > insertionIndex; --i)
            selectedPointIndices[i] = selectedPointIndices[i - 1];
        selectedPointIndices[insertionIndex] = pointIndex;
        ++selectedPointCount;
    }

    void sortSelectedPoints() noexcept
    {
        for (int i = 1; i < selectedPointCount; ++i) {
            const int value = selectedPointIndices[i];
            int j = i;
            while (j > 0 && selectedPointIndices[j - 1] > value) {
                selectedPointIndices[j] = selectedPointIndices[j - 1];
                --j;
            }
            selectedPointIndices[j] = value;
        }
    }
};
