#pragma once

#include <limits>

#include <CS2/Classes/Vector.h>
#include <Features/Visuals/GrenadePrediction/Trajectory.h>
#include <Utils/Math.h>

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
        intervalHeapSize = 0;

        if (trajectory.pointsCount < 2 || trajectory.pointsCount > Trajectory::kPointsCapacity
            || trajectory.markersCount < 0 || trajectory.markersCount > Trajectory::kMarkersCapacity)
            return;

        const int markerPanelCount = trajectory.markersCount + (trajectory.validLanding ? 1 : 0);
        const int maximumSelectedPointCount = kPanelCapacity - markerPanelCount + 1;
        if (trajectory.pointsCount <= maximumSelectedPointCount) {
            selectEveryPoint(trajectory.pointsCount);
            return;
        }

        addMandatoryPoints(trajectory);
        if (selectedPointCount >= maximumSelectedPointCount) {
            lineSegmentCount = selectedPointCount - 1;
            return;
        }

        for (int i = 1; i < selectedPointCount; ++i)
            pushInterval(trajectory, selectedPointIndices[i - 1], selectedPointIndices[i]);

        while (selectedPointCount < maximumSelectedPointCount && intervalHeapSize > 0) {
            const auto interval = popInterval();
            addSelectedPoint(interval.splitPointIndex);
            pushInterval(trajectory, interval.firstPointIndex, interval.splitPointIndex);
            pushInterval(trajectory, interval.splitPointIndex, interval.lastPointIndex);
        }

        lineSegmentCount = selectedPointCount - 1;
    }

    [[nodiscard]] int pointIndex(int index) const noexcept
    {
        return selectedPointIndices[index];
    }

private:
    struct Interval {
        int firstPointIndex{};
        int lastPointIndex{};
        int splitPointIndex{};
        float maximumSquaredError{};
    };

    static constexpr int kIntervalCapacity = kSelectedPointsCapacity;

    void selectEveryPoint(int pointCount) noexcept
    {
        selectedPointCount = pointCount;
        for (int i = 0; i < pointCount; ++i)
            selectedPointIndices[i] = i;
        lineSegmentCount = pointCount - 1;
    }

    void addMandatoryPoints(const Trajectory& trajectory) noexcept
    {
        addSelectedPoint(0);
        addSelectedPoint(trajectory.pointsCount - 1);
        for (int i = 0; i < trajectory.markersCount; ++i) {
            const int pointIndex = trajectory.markers[i].pointIndex;
            if (pointIndex >= 0 && pointIndex < trajectory.pointsCount)
                addSelectedPoint(pointIndex);
        }
    }

    void addSelectedPoint(int pointIndex) noexcept
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

    void pushInterval(const Trajectory& trajectory, int firstPointIndex, int lastPointIndex) noexcept
    {
        if (lastPointIndex - firstPointIndex < 2 || intervalHeapSize == kIntervalCapacity)
            return;

        const auto interval = findIntervalWithMaximumError(trajectory, firstPointIndex, lastPointIndex);
        int insertionIndex = intervalHeapSize++;
        while (insertionIndex > 0) {
            const int parentIndex = (insertionIndex - 1) / 2;
            if (!hasHigherPriority(interval, intervalHeap[parentIndex]))
                break;
            intervalHeap[insertionIndex] = intervalHeap[parentIndex];
            insertionIndex = parentIndex;
        }
        intervalHeap[insertionIndex] = interval;
    }

    [[nodiscard]] Interval popInterval() noexcept
    {
        const Interval result = intervalHeap[0];
        const Interval lastInterval = intervalHeap[--intervalHeapSize];
        if (intervalHeapSize == 0)
            return result;

        int parentIndex = 0;
        while (true) {
            const int leftChildIndex = parentIndex * 2 + 1;
            if (leftChildIndex >= intervalHeapSize)
                break;
            const int rightChildIndex = leftChildIndex + 1;
            int higherPriorityChildIndex = leftChildIndex;
            if (rightChildIndex < intervalHeapSize && hasHigherPriority(intervalHeap[rightChildIndex], intervalHeap[leftChildIndex]))
                higherPriorityChildIndex = rightChildIndex;
            if (!hasHigherPriority(intervalHeap[higherPriorityChildIndex], lastInterval))
                break;
            intervalHeap[parentIndex] = intervalHeap[higherPriorityChildIndex];
            parentIndex = higherPriorityChildIndex;
        }
        intervalHeap[parentIndex] = lastInterval;
        return result;
    }

    [[nodiscard]] static bool hasHigherPriority(const Interval& first, const Interval& second) noexcept
    {
        if (first.maximumSquaredError != second.maximumSquaredError)
            return first.maximumSquaredError > second.maximumSquaredError;

        const int firstSpan = first.lastPointIndex - first.firstPointIndex;
        const int secondSpan = second.lastPointIndex - second.firstPointIndex;
        if (firstSpan != secondSpan)
            return firstSpan > secondSpan;
        return first.firstPointIndex < second.firstPointIndex;
    }

    [[nodiscard]] static Interval findIntervalWithMaximumError(const Trajectory& trajectory, int firstPointIndex,
        int lastPointIndex) noexcept
    {
        Interval interval{.firstPointIndex = firstPointIndex, .lastPointIndex = lastPointIndex,
            .splitPointIndex = firstPointIndex + (lastPointIndex - firstPointIndex) / 2};
        int bestSplitBalance = lastPointIndex - firstPointIndex;
        bool hasCandidate = false;
        for (int pointIndex = firstPointIndex + 1; pointIndex < lastPointIndex; ++pointIndex) {
            const float squaredError = getPointToChordDistanceSquared(trajectory, firstPointIndex, lastPointIndex, pointIndex);
            const int splitBalance = getSplitBalance(firstPointIndex, lastPointIndex, pointIndex);
            if (!hasCandidate || squaredError > interval.maximumSquaredError
                || (squaredError == interval.maximumSquaredError && splitBalance < bestSplitBalance)) {
                interval.splitPointIndex = pointIndex;
                interval.maximumSquaredError = squaredError;
                bestSplitBalance = splitBalance;
                hasCandidate = true;
            }
        }
        return interval;
    }

    [[nodiscard]] static int getSplitBalance(int firstPointIndex, int lastPointIndex, int splitPointIndex) noexcept
    {
        const int leftSpan = splitPointIndex - firstPointIndex;
        const int rightSpan = lastPointIndex - splitPointIndex;
        return leftSpan > rightSpan ? leftSpan - rightSpan : rightSpan - leftSpan;
    }

    [[nodiscard]] static float getPointToChordDistanceSquared(const Trajectory& trajectory, int firstPointIndex,
        int lastPointIndex, int pointIndex) noexcept
    {
        const auto& first = trajectory.points[firstPointIndex];
        const auto& last = trajectory.points[lastPointIndex];
        const auto& point = trajectory.points[pointIndex];
        if (!hasFiniteCoordinates(first) || !hasFiniteCoordinates(last) || !hasFiniteCoordinates(point))
            return (std::numeric_limits<float>::max)();

        const float chordX = last.x - first.x;
        const float chordY = last.y - first.y;
        const float chordZ = last.z - first.z;
        const float chordLengthSquared = chordX * chordX + chordY * chordY + chordZ * chordZ;
        if (!Math::isFinite(chordLengthSquared))
            return (std::numeric_limits<float>::max)();

        float projection = 0.0f;
        if (chordLengthSquared > 0.0f) {
            const float pointX = point.x - first.x;
            const float pointY = point.y - first.y;
            const float pointZ = point.z - first.z;
            const float dotProduct = pointX * chordX + pointY * chordY + pointZ * chordZ;
            if (!Math::isFinite(dotProduct))
                return (std::numeric_limits<float>::max)();
            projection = dotProduct / chordLengthSquared;
            if (!Math::isFinite(projection))
                return (std::numeric_limits<float>::max)();
            if (projection < 0.0f)
                projection = 0.0f;
            else if (projection > 1.0f)
                projection = 1.0f;
        }

        const float errorX = point.x - (first.x + chordX * projection);
        const float errorY = point.y - (first.y + chordY * projection);
        const float errorZ = point.z - (first.z + chordZ * projection);
        const float squaredError = errorX * errorX + errorY * errorY + errorZ * errorZ;
        return Math::isFinite(squaredError) ? squaredError : (std::numeric_limits<float>::max)();
    }

    [[nodiscard]] static bool hasFiniteCoordinates(const cs2::Vector& point) noexcept
    {
        return Math::isFinite(point.x) && Math::isFinite(point.y) && Math::isFinite(point.z);
    }

    Interval intervalHeap[kIntervalCapacity]{};
    int intervalHeapSize{};
};
