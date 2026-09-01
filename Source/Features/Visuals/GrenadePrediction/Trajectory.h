#pragma once

#include <CS2/Classes/Vector.h>
#include <Utils/Math.h>

enum class TrajectoryMarkerKind {
    WorldContact,
    PlayerResponse
};

struct TrajectoryMarker {
    int pointIndex{};
    TrajectoryMarkerKind kind{TrajectoryMarkerKind::WorldContact};
};

struct Trajectory {
    static constexpr int kPointsCapacity = 500;
    static constexpr int kWorldContactMarkersCapacity = 20;
    static constexpr int kMarkersCapacity = kWorldContactMarkersCapacity + 1;

    int pointsCount{};
    cs2::Vector points[kPointsCapacity]{};
    float elapsedTimes[kPointsCapacity]{};
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
        const float elapsedTime = pointsCount ? elapsedTimes[pointsCount - 1] + 1.0f : 0.0f;
        return appendPoint(point, elapsedTime);
    }

    [[nodiscard]] bool appendPoint(cs2::Vector point, float elapsedTime) noexcept
    {
        if (pointsCount == kPointsCapacity || !Math::isFinite(elapsedTime) || (pointsCount && elapsedTime <= elapsedTimes[pointsCount - 1]))
            return false;
        points[pointsCount] = point;
        elapsedTimes[pointsCount++] = elapsedTime;
        return true;
    }

    [[nodiscard]] bool appendWorldContactMarker() noexcept
    {
        if (worldContactMarkersCount == kWorldContactMarkersCapacity || !appendMarker(TrajectoryMarkerKind::WorldContact))
            return false;
        ++worldContactMarkersCount;
        return true;
    }

    [[nodiscard]] bool appendPlayerResponseMarker() noexcept
    {
        return appendMarker(TrajectoryMarkerKind::PlayerResponse);
    }

private:
    [[nodiscard]] bool appendMarker(TrajectoryMarkerKind kind) noexcept
    {
        if (!pointsCount || markersCount == kMarkersCapacity)
            return false;
        markers[markersCount++] = {pointsCount - 1, kind};
        return true;
    }
};
