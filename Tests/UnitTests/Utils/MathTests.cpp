#include <bit>
#include <cstdint>

#include <gtest/gtest.h>

#include <Utils/Math.h>

namespace
{

    constexpr auto kNaN = std::bit_cast<float>(std::uint32_t{0x7FC00000});
    constexpr auto kInfinity = std::bit_cast<float>(std::uint32_t{0x7F800000});

    static_assert(Math::isFinite(0.0f));
    static_assert(!Math::isFinite(kNaN));
    static_assert(!Math::isFinite(kInfinity));

    TEST(MathTest, IdentifiesFiniteFloatValues)
    {
        EXPECT_TRUE(Math::isFinite(-1.0f));
        EXPECT_TRUE(Math::isFinite(1.0f));
        EXPECT_FALSE(Math::isFinite(kNaN));
        EXPECT_FALSE(Math::isFinite(kInfinity));
    }

}
