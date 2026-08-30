#pragma once

#include <CS2/Classes/Vector.h>

struct TrajectoryMarker {
    int pointIndex{};
};

struct Trajectory {
    static constexpr int kPointsCapacity = 500;
    static constexpr int kWorldContactMarkersCapacity = 20;
    static constexpr int kMarkersCapacity = kWorldContactMarkersCapacity + 1;

    int pointsCount{};
    cs2::Vector points[kPointsCapacity]{};
    int markersCount{};
    int worldContactMarkersCount{};
    TrajectoryMarker markers[kMarkersCapacity]{};
    cs2::Vector endPos{};
    bool valid{};
    bool validLanding{true};

    void clear() noexcept
    {
        pointsCount = 0;
        markersCount = 0;
        worldContactMarkersCount = 0;
        endPos = {};
        valid = false;
        validLanding = true;
    }

    [[nodiscard]] bool appendPoint(cs2::Vector point) noexcept
    {
        if (pointsCount == kPointsCapacity)
            return false;
        points[pointsCount++] = point;
        return true;
    }

    [[nodiscard]] bool appendWorldContactMarker() noexcept
    {
        if (worldContactMarkersCount == kWorldContactMarkersCapacity || !appendMarker())
            return false;
        ++worldContactMarkersCount;
        return true;
    }

    [[nodiscard]] bool appendPlayerResponseMarker() noexcept
    {
        return appendMarker();
    }

private:
    [[nodiscard]] bool appendMarker() noexcept
    {
        if (!pointsCount || markersCount == kMarkersCapacity)
            return false;
        markers[markersCount++] = {pointsCount - 1};
        return true;
    }
};
