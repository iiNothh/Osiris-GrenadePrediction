#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionMirror.h>
#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionState.h>

namespace
{

[[nodiscard]] GrenadePlayerCollisionSnapshot availablePlayerCollisionSnapshot() noexcept
{
    GrenadePlayerCollisionSnapshot snapshot;
    snapshot.status = GrenadePlayerCollisionSnapshotStatus::Available;
    return snapshot;
}

TEST(GrenadePlayerCollisionSelectionTest, DeeperOverlapBeatsLowerHandle)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {2.0f, -1.0f, -1.0f}, {4.0f, 1.0f, 1.0f}, true};
    snapshot.candidates[snapshot.count++] = {100, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};

    const auto* const selected = grenade_player_collision_mirror::select(snapshot, {});

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->rawHandle, 100);
}

TEST(GrenadePlayerCollisionSelectionTest, NestedContainingBoundsUseNearestCenter)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {-10.0f, -10.0f, -10.0f}, {10.0f, 10.0f, 10.0f}, true};
    snapshot.candidates[snapshot.count++] = {100, {0.0f, -1.0f, -1.0f}, {2.0f, 1.0f, 1.0f}, true};

    const auto* const selected = grenade_player_collision_mirror::select(snapshot, {1.0f, 0.0f, 0.0f});

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->rawHandle, 100);
}

TEST(GrenadePlayerCollisionSelectionTest, RanksActualCenterDistanceInsteadOfLargestAxisDistance)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {-4.0f, -4.0f, -4.0f}, {0.0f, 0.0f, 0.0f}, true};
    snapshot.candidates[snapshot.count++] = {100, {-4.4f, -1.0f, -1.0f}, {0.0f, 1.0f, 1.0f}, true};

    const auto* const selected = grenade_player_collision_mirror::select(snapshot, {});

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->rawHandle, 100);
}

TEST(GrenadePlayerCollisionSelectionTest, ReversingSnapshotOrderDoesNotChangeSelection)
{
    auto firstSnapshot = availablePlayerCollisionSnapshot();
    firstSnapshot.candidates[firstSnapshot.count++] = {1, {2.0f, -1.0f, -1.0f}, {4.0f, 1.0f, 1.0f}, true};
    firstSnapshot.candidates[firstSnapshot.count++] = {100, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};
    auto secondSnapshot = availablePlayerCollisionSnapshot();
    secondSnapshot.candidates[secondSnapshot.count++] = firstSnapshot.candidates[1];
    secondSnapshot.candidates[secondSnapshot.count++] = firstSnapshot.candidates[0];

    const auto* const firstSelection = grenade_player_collision_mirror::select(firstSnapshot, {});
    const auto* const secondSelection = grenade_player_collision_mirror::select(secondSnapshot, {});

    ASSERT_NE(firstSelection, nullptr);
    ASSERT_NE(secondSelection, nullptr);
    EXPECT_EQ(firstSelection->rawHandle, 100);
    EXPECT_EQ(secondSelection->rawHandle, firstSelection->rawHandle);
}

TEST(GrenadePlayerCollisionSelectionTest, IdenticalGeometryUsesLowestHandle)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {100, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};
    snapshot.candidates[snapshot.count++] = {1, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};

    const auto* const selected = grenade_player_collision_mirror::select(snapshot, {});

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->rawHandle, 1);
}

TEST(GrenadePlayerCollisionSelectionTest, SkipsIneligibleCandidate)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, false};
    snapshot.candidates[snapshot.count++] = {100, {-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f}, true};

    const auto* const selected = grenade_player_collision_mirror::select(snapshot, {});

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->rawHandle, 100);
}

TEST(GrenadePlayerCollisionSelectionTest, FailsClosedForUnavailableOrInvalidInput)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {1, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};

    snapshot.status = GrenadePlayerCollisionSnapshotStatus::Unavailable;
    EXPECT_EQ(grenade_player_collision_mirror::select(snapshot, {}), nullptr);
    snapshot.status = GrenadePlayerCollisionSnapshotStatus::Available;
    snapshot.count = -1;
    EXPECT_EQ(grenade_player_collision_mirror::select(snapshot, {}), nullptr);
    snapshot.count = GrenadePlayerCollisionSnapshot::kCapacity + 1;
    EXPECT_EQ(grenade_player_collision_mirror::select(snapshot, {}), nullptr);
    snapshot.count = 1;
    EXPECT_EQ(grenade_player_collision_mirror::select(snapshot, {std::numeric_limits<float>::infinity(), 0.0f, 0.0f}), nullptr);
    snapshot.candidates[0].mins.x = 2.0f;
    EXPECT_EQ(grenade_player_collision_mirror::select(snapshot, {}), nullptr);
    snapshot.candidates[0] = {1, {-1.0f, -1.0f, -1.0f}, {std::numeric_limits<float>::infinity(), 1.0f, 1.0f}, true};
    EXPECT_EQ(grenade_player_collision_mirror::select(snapshot, {}), nullptr);
}

TEST(GrenadePlayerCollisionSelectionTest, CoCenteredNestedBoundsUseLowestHandle)
{
    auto snapshot = availablePlayerCollisionSnapshot();
    snapshot.candidates[snapshot.count++] = {100, {-10.0f, -10.0f, -10.0f}, {10.0f, 10.0f, 10.0f}, true};
    snapshot.candidates[snapshot.count++] = {1, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};

    const auto* const selected = grenade_player_collision_mirror::select(snapshot, {});

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->rawHandle, 1);
}

}
