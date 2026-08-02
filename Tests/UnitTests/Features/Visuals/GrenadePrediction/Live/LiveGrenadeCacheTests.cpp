#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>

namespace
{

[[nodiscard]] const grenade_prediction::LiveGrenadeSnapshot* snapshotWithHandle(const grenade_prediction::LiveGrenadeCache& cache, cs2::CEntityHandle handle) noexcept
{
    for (const auto& snapshot : cache.snapshots()) {
        if (snapshot.handle == handle)
            return &snapshot;
    }
    return nullptr;
}

class LiveGrenadeCacheTest : public testing::Test {
protected:
    grenade_prediction::LiveGrenadeCacheState state;
    grenade_prediction::LiveGrenadeCache cache{state};
};

TEST_F(LiveGrenadeCacheTest, UsesFullHandleForIdentity)
{
    cache.beginScan();
    cache.observe({{1}, {2}, {}, {}, grenade_prediction::GrenadeKind::Flashbang});
    cache.observe({{0x8001}, {2}, {}, {}, grenade_prediction::GrenadeKind::HEGrenade});
    cache.endScan();

    EXPECT_EQ(cache.snapshots().getSize(), 2);
    const auto* first = snapshotWithHandle(cache, {1});
    const auto* second = snapshotWithHandle(cache, {0x8001});
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
    EXPECT_TRUE(cache.contains({1}, {2}, first->firstObservationSequence));
    EXPECT_FALSE(cache.contains({1}, {2}, second->firstObservationSequence));
}

TEST_F(LiveGrenadeCacheTest, PreservesSequenceAcrossRefreshAndPrunesOnlyAtEndScan)
{
    cache.beginScan();
    cache.observe({{9}, {3}, {}, {}, grenade_prediction::GrenadeKind::Flashbang});
    cache.endScan();
    const auto sequence = snapshotWithHandle(cache, {9})->firstObservationSequence;

    cache.beginScan();
    cache.observe({{9}, {3}, {1.0f, 0.0f, 0.0f}, {}, grenade_prediction::GrenadeKind::Flashbang});
    cache.observe({{2}, {3}, {}, {}, grenade_prediction::GrenadeKind::SmokeGrenade});
    EXPECT_EQ(snapshotWithHandle(cache, {9})->firstObservationSequence, sequence);
    EXPECT_GT(snapshotWithHandle(cache, {2})->firstObservationSequence, sequence);
    cache.endScan();

    cache.beginScan();
    cache.observe({{2}, {3}, {}, {}, grenade_prediction::GrenadeKind::SmokeGrenade});
    cache.endScan();
    EXPECT_EQ(snapshotWithHandle(cache, {9}), nullptr);
    cache.clear();
    EXPECT_EQ(cache.snapshots().getSize(), 0);
    EXPECT_EQ(state.nextFirstObservationSequence, 1);
}

TEST_F(LiveGrenadeCacheTest, SelectsNewestThrowerSnapshotByObservationSequenceNotHandle)
{
    cache.beginScan();
    cache.observe({{0xFFFF}, {7}, {}, {}, grenade_prediction::GrenadeKind::Flashbang});
    cache.observe({{1}, {7}, {}, {}, grenade_prediction::GrenadeKind::HEGrenade});
    cache.observe({{2}, {8}, {}, {}, grenade_prediction::GrenadeKind::SmokeGrenade});
    cache.endScan();

    ASSERT_NE(cache.newestForThrower({7}), nullptr);
    EXPECT_EQ(cache.newestForThrower({7})->handle, (cs2::CEntityHandle{1}));
    EXPECT_EQ(cache.newestForThrower({7})->firstObservationSequence, 2U);
}

TEST_F(LiveGrenadeCacheTest, RemovesExpiredSmokeAndDecoySnapshots)
{
    cache.beginScan();
    cache.observe({{4}, {7}, {}, {}, grenade_prediction::GrenadeKind::SmokeGrenade});
    cache.observe({{5}, {7}, {}, {}, grenade_prediction::GrenadeKind::Decoy});
    cache.endScan();

    cache.remove({4});
    cache.remove({5});

    EXPECT_EQ(cache.newestForThrower({7}), nullptr);
}

}
