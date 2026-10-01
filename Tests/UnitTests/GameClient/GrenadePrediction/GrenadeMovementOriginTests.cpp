#include <array>
#include <bit>
#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include <GameClient/GrenadePrediction/GrenadeMovementOrigin.h>

namespace
{

constexpr float kMeasuredMaxCoord{16384.0f};
constexpr float kEpsilon{std::bit_cast<float>(std::uint32_t{0x3B800000})};

struct OriginCase {
    cs2::Vector old;
    cs2::Vector candidate;
    bool retained;
};

constexpr OriginCase kOriginCases[]{
    {{}, {std::bit_cast<float>(std::uint32_t{0x3B7FFFFF}), 0.0f, 0.0f}, true},
    {{}, {kEpsilon, 0.0f, 0.0f}, true},
    {{}, {std::bit_cast<float>(std::uint32_t{0x3B800001}), 0.0f, 0.0f}, false},
    {{}, {0.003f, 0.003f, 0.003f}, true},
    {{}, {0.001f, -0.002f, 0.01f}, false},
    {{}, {0.001f, 0.01f, -0.002f}, false},
    {{}, {0.01f, 0.001f, -0.002f}, false},
    {{65536.0f, 0.0f, 0.0f}, {65536.015625f, 0.0f, 0.0f}, true},
    {{65536.0f, 0.0f, 0.0f}, {65536.0234375f, 0.0f, 0.0f}, false}
};

class GrenadeMovementOriginBoundaryTest : public testing::TestWithParam<OriginCase> {
};

TEST_P(GrenadeMovementOriginBoundaryTest, UsesInclusiveComponentThresholdAndCommitsWholeCandidate)
{
    const auto& test = GetParam();
    auto stored = test.old;

    EXPECT_TRUE(GrenadeMovementOrigin::commit(stored, test.candidate, kMeasuredMaxCoord));
    EXPECT_EQ(stored, test.retained ? test.old : test.candidate);
}

INSTANTIATE_TEST_SUITE_P(Boundaries, GrenadeMovementOriginBoundaryTest, testing::ValuesIn(kOriginCases));

TEST(GrenadeMovementOriginTest, RetainsSignedZeroBitsOnAllNearComparison)
{
    cs2::Vector stored{-0.0f, 0.0f, -0.0f};

    EXPECT_TRUE(GrenadeMovementOrigin::commit(stored, {0.0f, -0.0f, 0.0f}, kMeasuredMaxCoord));
    EXPECT_EQ(std::bit_cast<std::uint32_t>(stored.x), 0x80000000u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(stored.y), 0u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(stored.z), 0x80000000u);
}

TEST(GrenadeMovementOriginTest, RecomputesRelativeThresholdForChangedMaxCoord)
{
    cs2::Vector stored{65536.0f, 0.0f, 0.0f};
    const cs2::Vector candidate{65536.015625f, 0.0f, 0.0f};

    EXPECT_TRUE(GrenadeMovementOrigin::commit(stored, candidate, kMeasuredMaxCoord));
    EXPECT_EQ(stored.x, 65536.0f);
    EXPECT_TRUE(GrenadeMovementOrigin::commit(stored, candidate, 32768.0f));
    EXPECT_EQ(stored, candidate);
}

class GrenadeMovementOriginInvalidCandidateTest : public testing::TestWithParam<std::array<std::uint32_t, 3>> {
};

TEST_P(GrenadeMovementOriginInvalidCandidateTest, RejectsWholeCandidateWithoutChangingStoredBits)
{
    cs2::Vector stored{-0.0f, 2.0f, 3.0f};
    const auto& bits = GetParam();
    const cs2::Vector candidate{std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};

    EXPECT_FALSE(GrenadeMovementOrigin::commit(stored, candidate, kMeasuredMaxCoord));
    EXPECT_EQ(std::bit_cast<std::uint32_t>(stored.x), 0x80000000u);
    EXPECT_EQ(stored.y, 2.0f);
    EXPECT_EQ(stored.z, 3.0f);
}

INSTANTIATE_TEST_SUITE_P(Nonfinite, GrenadeMovementOriginInvalidCandidateTest, testing::Values(
    std::array<std::uint32_t, 3>{0x7FC00000u, 0u, 0u},
    std::array<std::uint32_t, 3>{0u, 0x7F800000u, 0u},
    std::array<std::uint32_t, 3>{0u, 0u, 0xFF800000u},
    std::array<std::uint32_t, 3>{0u, 0u, 0x7F800001u}));

TEST(GrenadeMovementOriginTest, OrderedComparisonDoesNotRetainNonfiniteOldOrigin)
{
    cs2::Vector stored{std::numeric_limits<float>::quiet_NaN(), 2.0f, 3.0f};
    const cs2::Vector candidate{1.0f, 2.0f, 3.0f};

    EXPECT_TRUE(GrenadeMovementOrigin::commit(stored, candidate, kMeasuredMaxCoord));
    EXPECT_EQ(stored, candidate);
}

class GrenadeMovementOriginInvalidScaleTest : public testing::TestWithParam<float> {
};

TEST_P(GrenadeMovementOriginInvalidScaleTest, FailsWithoutChangingStoredOrigin)
{
    cs2::Vector stored{1.0f, 2.0f, 3.0f};

    EXPECT_FALSE(GrenadeMovementOrigin::commit(stored, {4.0f, 5.0f, 6.0f}, GetParam()));
    EXPECT_EQ(stored, (cs2::Vector{1.0f, 2.0f, 3.0f}));
}

INSTANTIATE_TEST_SUITE_P(UnsupportedScale, GrenadeMovementOriginInvalidScaleTest, testing::Values(
    0.0f, -0.0f, -1.0f, std::numeric_limits<float>::infinity(),
    -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()));

}
