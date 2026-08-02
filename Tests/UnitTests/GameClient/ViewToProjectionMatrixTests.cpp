#include <bit>
#include <cstdint>

#include <gtest/gtest.h>

#include <GameClient/WorldToScreen/ViewToProjectionMatrix.h>

namespace
{

struct PatternSearchResults {
    cs2::VMatrix* matrix{};

    template <typename>
    [[nodiscard]] cs2::VMatrix* get() const noexcept
    {
        return matrix;
    }
};

struct HookContext {
    PatternSearchResults results{};

    [[nodiscard]] const PatternSearchResults& patternSearchResults() const noexcept
    {
        return results;
    }
};

TEST(ViewToProjectionMatrixTest, ReturnsAspectRatioFromFiniteProjectionTerms)
{
    cs2::VMatrix matrix = cs2::VMatrix::identity();
    matrix.m[0][0] = 0.75f;
    matrix.m[1][1] = 1.0f;
    HookContext hookContext{{&matrix}};

    EXPECT_FLOAT_EQ(ViewToProjectionMatrix{hookContext}.getAspectRatio(), 4.0f / 3.0f);
}

TEST(ViewToProjectionMatrixTest, FallsBackForMissingOrInvalidProjectionTerms)
{
    HookContext hookContext{};
    EXPECT_FLOAT_EQ(ViewToProjectionMatrix{hookContext}.getAspectRatio(), cs2::kDefaultAspectRatio);

    cs2::VMatrix matrix = cs2::VMatrix::identity();
    matrix.m[0][0] = 0.0f;
    hookContext.results.matrix = &matrix;
    EXPECT_FLOAT_EQ(ViewToProjectionMatrix{hookContext}.getAspectRatio(), cs2::kDefaultAspectRatio);

    matrix.m[0][0] = std::bit_cast<float>(std::uint32_t{0x7F800000u});
    EXPECT_FLOAT_EQ(ViewToProjectionMatrix{hookContext}.getAspectRatio(), cs2::kDefaultAspectRatio);
}

}
