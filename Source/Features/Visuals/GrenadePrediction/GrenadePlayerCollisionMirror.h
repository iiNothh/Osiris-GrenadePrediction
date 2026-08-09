#pragma once

#include <cstdint>

#include <CS2/Classes/Vector.h>
#include <Utils/Math.h>

struct GrenadePlayerCollisionCandidate {
    std::uint32_t rawHandle{};
    cs2::Vector mins{};
    cs2::Vector maxs{};
    bool relationshipEligible{};
};

struct GrenadePlayerCollisionSnapshot {
    static constexpr int kCapacity = 48;
    GrenadePlayerCollisionCandidate candidates[kCapacity]{};
    int count{};
    bool available{};

    void reset() noexcept
    {
        count = 0;
        available = false;
    }

    [[nodiscard]] bool append(GrenadePlayerCollisionCandidate candidate) noexcept
    {
        if (count == kCapacity)
            return false;
        candidates[count++] = candidate;
        return true;
    }
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

    [[nodiscard]] inline const GrenadePlayerCollisionCandidate* select(const GrenadePlayerCollisionSnapshot& snapshot, cs2::Vector origin) noexcept
    {
        if (!snapshot.available || snapshot.count < 0 || snapshot.count > GrenadePlayerCollisionSnapshot::kCapacity)
            return nullptr;

        const GrenadePlayerCollisionCandidate* selected{};
        for (int i = 0; i < snapshot.count; ++i) {
            const auto& candidate = snapshot.candidates[i];
            if (candidate.relationshipEligible && intersectsSphere(origin, candidate)
                && (!selected || candidate.rawHandle < selected->rawHandle))
                selected = &candidate;
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
