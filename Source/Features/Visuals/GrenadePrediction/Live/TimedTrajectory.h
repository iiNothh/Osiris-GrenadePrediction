#pragma once

#include <Features/Visuals/GrenadePrediction/Trajectory.h>
#include <Utils/Optional.h>

namespace timed_trajectory
{

struct Join {
    int publishedPointIndex;
    int candidatePointIndex;
    cs2::Vector position;
    float elapsedTime;
};

[[nodiscard]] inline bool finite(const cs2::Vector& value) noexcept;
[[nodiscard]] inline bool hasValidPointMetadata(const Trajectory& trajectory) noexcept;
[[nodiscard]] inline bool hasValidTrajectoryMetadata(const Trajectory& trajectory) noexcept;
[[nodiscard]] inline bool countHistoricalMarkers(const Trajectory& trajectory, int joinPointIndex, int& markersCount,
    int& worldContactMarkersCount) noexcept;
[[nodiscard]] inline bool countSuffixMarkers(const Trajectory& trajectory, int joinPointIndex, int& markersCount,
    int& worldContactMarkersCount) noexcept;
inline void copyHistoricalMarkers(Trajectory& trajectory, int joinPointIndex, int& destinationMarkerIndex) noexcept;
inline void copySuffixMarkers(Trajectory& destination, const Trajectory& candidate, int prefixCount, int candidateJoinPointIndex,
    int& destinationMarkerIndex) noexcept;

[[nodiscard]] inline Optional<cs2::Vector> samplePosition(const Trajectory& trajectory, float elapsedTime) noexcept
{
    if (!hasValidPointMetadata(trajectory) || !Math::isFinite(elapsedTime) || elapsedTime < trajectory.elapsedTimes[0]
        || elapsedTime > trajectory.elapsedTimes[trajectory.pointsCount - 1])
        return {};

    for (int i = 0; i < trajectory.pointsCount; ++i) {
        if (elapsedTime == trajectory.elapsedTimes[i])
            return trajectory.points[i];
        if (elapsedTime < trajectory.elapsedTimes[i]) {
            const float duration = trajectory.elapsedTimes[i] - trajectory.elapsedTimes[i - 1];
            const float fraction = (elapsedTime - trajectory.elapsedTimes[i - 1]) / duration;
            const auto position = trajectory.points[i - 1] + (trajectory.points[i] - trajectory.points[i - 1]) * fraction;
            if (Math::isFinite(fraction) && fraction > 0.0f && fraction < 1.0f && finite(position))
                return position;
            return {};
        }
    }
    return {};
}

[[nodiscard]] inline bool replaceFuture(Trajectory& published, const Trajectory& candidate, Join join) noexcept
{
    if (&published == &candidate || !hasValidTrajectoryMetadata(published) || !hasValidTrajectoryMetadata(candidate)
        || join.publishedPointIndex < 0 || join.publishedPointIndex >= published.pointsCount
        || join.candidatePointIndex < 0 || join.candidatePointIndex >= candidate.pointsCount
        || !finite(join.position) || !Math::isFinite(join.elapsedTime) || join.elapsedTime < 0.0f)
        return false;

    const int prefixCount = join.publishedPointIndex;
    const int suffixCount = candidate.pointsCount - join.candidatePointIndex - 1;
    const int pointsCount = prefixCount + 1 + suffixCount;
    if (pointsCount > Trajectory::kPointsCapacity || (prefixCount && join.elapsedTime <= published.elapsedTimes[prefixCount - 1]))
        return false;

    const float elapsedOffset = join.elapsedTime - candidate.elapsedTimes[join.candidatePointIndex];
    if (!Math::isFinite(elapsedOffset))
        return false;

    float previousElapsedTime = join.elapsedTime;
    for (int i = join.candidatePointIndex + 1; i < candidate.pointsCount; ++i) {
        const float elapsedTime = candidate.elapsedTimes[i] + elapsedOffset;
        if (!Math::isFinite(elapsedTime) || elapsedTime <= previousElapsedTime)
            return false;
        previousElapsedTime = elapsedTime;
    }

    int markersCount{};
    int worldContactMarkersCount{};
    if (!countHistoricalMarkers(published, join.publishedPointIndex, markersCount, worldContactMarkersCount)
        || !countSuffixMarkers(candidate, join.candidatePointIndex, markersCount, worldContactMarkersCount))
        return false;

    published.points[prefixCount] = join.position;
    published.elapsedTimes[prefixCount] = join.elapsedTime;
    for (int sourceIndex = join.candidatePointIndex + 1, destinationIndex = prefixCount + 1; sourceIndex < candidate.pointsCount; ++sourceIndex, ++destinationIndex) {
        published.points[destinationIndex] = candidate.points[sourceIndex];
        published.elapsedTimes[destinationIndex] = candidate.elapsedTimes[sourceIndex] + elapsedOffset;
    }

    int destinationMarkerIndex{};
    copyHistoricalMarkers(published, join.publishedPointIndex, destinationMarkerIndex);
    copySuffixMarkers(published, candidate, join.publishedPointIndex, join.candidatePointIndex, destinationMarkerIndex);
    published.pointsCount = pointsCount;
    published.markersCount = markersCount;
    published.worldContactMarkersCount = worldContactMarkersCount;
    published.endPos = candidate.endPos;
    published.valid = candidate.valid;
    published.validLanding = candidate.validLanding;
    return true;
}

[[nodiscard]] inline bool finite(const cs2::Vector& value) noexcept
{
    return Math::isFinite(value.x) && Math::isFinite(value.y) && Math::isFinite(value.z);
}

[[nodiscard]] inline bool hasValidPointMetadata(const Trajectory& trajectory) noexcept
{
    if (trajectory.pointsCount <= 0 || trajectory.pointsCount > Trajectory::kPointsCapacity)
        return false;

    for (int i = 0; i < trajectory.pointsCount; ++i) {
        if (!finite(trajectory.points[i]) || !Math::isFinite(trajectory.elapsedTimes[i]) || trajectory.elapsedTimes[i] < 0.0f
            || (i && trajectory.elapsedTimes[i] <= trajectory.elapsedTimes[i - 1]))
            return false;
    }
    return true;
}

[[nodiscard]] inline bool hasValidTrajectoryMetadata(const Trajectory& trajectory) noexcept
{
    if (!trajectory.valid || !finite(trajectory.endPos) || !hasValidPointMetadata(trajectory)
        || trajectory.markersCount < 0 || trajectory.markersCount > Trajectory::kMarkersCapacity
        || trajectory.worldContactMarkersCount < 0 || trajectory.worldContactMarkersCount > Trajectory::kWorldContactMarkersCapacity)
        return false;

    int worldContactMarkersCount{};
    for (int i = 0; i < trajectory.markersCount; ++i) {
        const auto& marker = trajectory.markers[i];
        if (marker.pointIndex < 0 || marker.pointIndex >= trajectory.pointsCount)
            return false;
        if (marker.kind == TrajectoryMarkerKind::WorldContact)
            ++worldContactMarkersCount;
        else if (marker.kind != TrajectoryMarkerKind::PlayerResponse)
            return false;
    }
    return worldContactMarkersCount == trajectory.worldContactMarkersCount;
}

[[nodiscard]] inline bool countHistoricalMarkers(const Trajectory& trajectory, int joinPointIndex, int& markersCount, int& worldContactMarkersCount) noexcept
{
    for (int i = 0; i < trajectory.markersCount; ++i) {
        const auto& marker = trajectory.markers[i];
        if (marker.pointIndex >= joinPointIndex)
            continue;
        if (++markersCount > Trajectory::kMarkersCapacity)
            return false;
        if (marker.kind == TrajectoryMarkerKind::WorldContact && ++worldContactMarkersCount > Trajectory::kWorldContactMarkersCapacity)
            return false;
    }
    return true;
}

[[nodiscard]] inline bool countSuffixMarkers(const Trajectory& trajectory, int joinPointIndex, int& markersCount, int& worldContactMarkersCount) noexcept
{
    for (int i = 0; i < trajectory.markersCount; ++i) {
        const auto& marker = trajectory.markers[i];
        if (marker.pointIndex <= joinPointIndex)
            continue;
        if (++markersCount > Trajectory::kMarkersCapacity)
            return false;
        if (marker.kind == TrajectoryMarkerKind::WorldContact && ++worldContactMarkersCount > Trajectory::kWorldContactMarkersCapacity)
            return false;
    }
    return true;
}

inline void copyHistoricalMarkers(Trajectory& trajectory, int joinPointIndex, int& destinationMarkerIndex) noexcept
{
    for (int i = 0; i < trajectory.markersCount; ++i) {
        const auto marker = trajectory.markers[i];
        if (marker.pointIndex < joinPointIndex)
            trajectory.markers[destinationMarkerIndex++] = marker;
    }
}

inline void copySuffixMarkers(Trajectory& destination, const Trajectory& candidate, int prefixCount, int candidateJoinPointIndex,
    int& destinationMarkerIndex) noexcept
{
    for (int i = 0; i < candidate.markersCount; ++i) {
        auto marker = candidate.markers[i];
        if (marker.pointIndex <= candidateJoinPointIndex)
            continue;
        marker.pointIndex += prefixCount - candidateJoinPointIndex;
        destination.markers[destinationMarkerIndex++] = marker;
    }
}

}
