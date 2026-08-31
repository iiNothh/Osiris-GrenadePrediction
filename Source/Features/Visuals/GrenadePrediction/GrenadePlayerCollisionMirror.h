#pragma once

#include <cstdint>

#include <CS2/Classes/Vector.h>
#include <GameClient/Entities/TeamNumber.h>
#include <Utils/Math.h>

struct GrenadePlayerCollisionCandidate {
    std::uint32_t rawHandle{};
    cs2::Vector mins{};
    cs2::Vector maxs{};
    bool relationshipEligible{};
};

enum class GrenadePlayerCollisionSnapshotStatus : std::uint8_t {
    Unavailable,
    Available
};

struct GrenadePlayerCollisionCollectedCandidate {
    std::uint32_t rawHandle{};
    cs2::Vector mins{};
    cs2::Vector maxs{};
    TeamNumber team{};
};

struct GrenadePlayerCollisionCollectionScratch {
    static constexpr int kCapacity = 48;

    GrenadePlayerCollisionCollectedCandidate candidates[kCapacity]{};
    int count{};
    bool playerDataInvalid{};
    bool overflowed{};

    void reset() noexcept
    {
        count = 0;
        playerDataInvalid = false;
        overflowed = false;
    }
};

struct GrenadePlayerCollisionSnapshot {
    static constexpr int kCapacity = GrenadePlayerCollisionCollectionScratch::kCapacity;
    GrenadePlayerCollisionCandidate candidates[kCapacity]{};
    int count{};
    GrenadePlayerCollisionSnapshotStatus status{GrenadePlayerCollisionSnapshotStatus::Unavailable};
    std::uint64_t revision{};
};

namespace grenade_player_collision_mirror
{
    constexpr float kRadius = 3.0f;

    [[nodiscard]] inline bool finite(cs2::Vector value) noexcept
    {
        return Math::isFinite(value.x) && Math::isFinite(value.y) && Math::isFinite(value.z);
    }

    [[nodiscard]] inline bool validBounds(const GrenadePlayerCollisionCandidate& candidate) noexcept
    {
        return finite(candidate.mins) && finite(candidate.maxs)
            && candidate.mins.x <= candidate.maxs.x && candidate.mins.y <= candidate.maxs.y && candidate.mins.z <= candidate.maxs.z;
    }

    [[nodiscard]] inline bool intersectsSphere(cs2::Vector origin, const GrenadePlayerCollisionCandidate& candidate) noexcept
    {
        if (!finite(origin) || !validBounds(candidate))
            return false;

        const float x = origin.x < candidate.mins.x ? candidate.mins.x - origin.x
            : origin.x > candidate.maxs.x ? origin.x - candidate.maxs.x
            : 0.0f;
        const float y = origin.y < candidate.mins.y ? candidate.mins.y - origin.y
            : origin.y > candidate.maxs.y ? origin.y - candidate.maxs.y
            : 0.0f;
        const float z = origin.z < candidate.mins.z ? candidate.mins.z - origin.z
            : origin.z > candidate.maxs.z ? origin.z - candidate.maxs.z
            : 0.0f;

        return x * x + y * y + z * z <= kRadius * kRadius;
    }

    [[nodiscard]] inline float pointToAabbDistanceSquared(cs2::Vector origin, const GrenadePlayerCollisionCandidate& candidate) noexcept
    {
        const float x = origin.x < candidate.mins.x ? candidate.mins.x - origin.x
            : origin.x > candidate.maxs.x ? origin.x - candidate.maxs.x
            : 0.0f;
        const float y = origin.y < candidate.mins.y ? candidate.mins.y - origin.y
            : origin.y > candidate.maxs.y ? origin.y - candidate.maxs.y
            : 0.0f;
        const float z = origin.z < candidate.mins.z ? candidate.mins.z - origin.z
            : origin.z > candidate.maxs.z ? origin.z - candidate.maxs.z
            : 0.0f;
        return x * x + y * y + z * z;
    }

    struct ScaledSquaredDistance {
        float scale{};
        float normalizedSquared{};
        bool valid{};
    };

    [[nodiscard]] inline ScaledSquaredDistance centerDistanceSquared(cs2::Vector origin, const GrenadePlayerCollisionCandidate& candidate) noexcept
    {
        const auto center = candidate.mins * 0.5f + candidate.maxs * 0.5f;
        const auto halfDifference = origin * 0.5f - center * 0.5f;
        if (!finite(center) || !finite(halfDifference))
            return {};
        const auto absolute = [](float value) noexcept { return value < 0.0f ? -value : value; };
        float scale = absolute(halfDifference.x);
        if (const float y = absolute(halfDifference.y); y > scale)
            scale = y;
        if (const float z = absolute(halfDifference.z); z > scale)
            scale = z;
        if (scale == 0.0f)
            return {0.0f, 0.0f, true};

        const float inverseScale = 1.0f / scale;
        const auto normalized = halfDifference * inverseScale;
        const float normalizedSquared = normalized.squareLength();
        return Math::isFinite(scale) && finite(normalized) && Math::isFinite(normalizedSquared)
            ? ScaledSquaredDistance{scale, normalizedSquared, true}
            : ScaledSquaredDistance{};
    }

    [[nodiscard]] inline int compare(const ScaledSquaredDistance& first, const ScaledSquaredDistance& second) noexcept
    {
        if (first.scale == 0.0f)
            return second.scale == 0.0f ? 0 : -1;
        if (second.scale == 0.0f)
            return 1;
        if (first.scale <= second.scale) {
            const float scaleRatio = first.scale / second.scale;
            const float firstScaledDistance = first.normalizedSquared * scaleRatio * scaleRatio;
            if (firstScaledDistance < second.normalizedSquared)
                return -1;
            if (firstScaledDistance > second.normalizedSquared)
                return 1;
            return 0;
        }
        const float scaleRatio = second.scale / first.scale;
        const float secondScaledDistance = second.normalizedSquared * scaleRatio * scaleRatio;
        if (first.normalizedSquared < secondScaledDistance)
            return -1;
        if (first.normalizedSquared > secondScaledDistance)
            return 1;
        return 0;
    }

    [[nodiscard]] inline const GrenadePlayerCollisionCandidate* select(const GrenadePlayerCollisionSnapshot& snapshot, cs2::Vector origin) noexcept
    {
        if (snapshot.status != GrenadePlayerCollisionSnapshotStatus::Available || snapshot.count < 0 || snapshot.count > GrenadePlayerCollisionSnapshot::kCapacity
            || !finite(origin))
            return nullptr;

        const GrenadePlayerCollisionCandidate* selected{};
        float selectedPointDistanceSquared{};
        ScaledSquaredDistance selectedCenterDistanceSquared{};
        for (int i = 0; i < snapshot.count; ++i) {
            const auto& candidate = snapshot.candidates[i];
            if (!candidate.relationshipEligible || !intersectsSphere(origin, candidate))
                continue;

            const float pointDistanceSquared = pointToAabbDistanceSquared(origin, candidate);
            const auto centerDistance = centerDistanceSquared(origin, candidate);
            if (!Math::isFinite(pointDistanceSquared) || !centerDistance.valid)
                continue;
            const bool closerToBounds = !selected || pointDistanceSquared < selectedPointDistanceSquared;
            const bool equallyCloseToBounds = selected && pointDistanceSquared == selectedPointDistanceSquared;
            const int centerDistanceComparison = equallyCloseToBounds ? compare(centerDistance, selectedCenterDistanceSquared) : 0;
            if (closerToBounds || (equallyCloseToBounds && (centerDistanceComparison < 0
                || (centerDistanceComparison == 0 && candidate.rawHandle < selected->rawHandle)))) {
                selected = &candidate;
                selectedPointDistanceSquared = pointDistanceSquared;
                selectedCenterDistanceSquared = centerDistance;
            }
        }
        return selected;
    }

    [[nodiscard]] inline bool apply(cs2::Vector origin, const GrenadePlayerCollisionCandidate& candidate, cs2::Vector& velocity) noexcept
    {
        if (!finite(origin) || !finite(velocity) || !validBounds(candidate))
            return false;

        const auto normal = origin - (candidate.mins + candidate.maxs) * 0.5f;
        const float normalSq = normal.squareLength();
        const float speedSq = velocity.squareLength();
        if (!(normalSq >= 0.0f) || !(speedSq > 0.0f) || !Math::isFinite(normalSq) || !Math::isFinite(speedSq))
            return false;

        if (normalSq == 0.0f) {
            velocity = velocity * 0.3f;
            return true;
        }

        const auto unitNormal = normal * (1.0f / Math::sqrt(normalSq));
        const auto reflected = velocity - unitNormal * (2.0f * velocity.dot(unitNormal));
        const float reflectedSq = reflected.squareLength();
        if (!(reflectedSq > 0.0f) || !finite(reflected) || !Math::isFinite(reflectedSq))
            return false;

        velocity = reflected * (Math::sqrt(speedSq) * 0.3f / Math::sqrt(reflectedSq));
        return finite(velocity);
    }
}
