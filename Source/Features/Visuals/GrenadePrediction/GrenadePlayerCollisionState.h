#pragma once

#include <cstdint>

#include <CS2/Classes/Vector.h>
#include <GameClient/Entities/TeamNumber.h>

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
