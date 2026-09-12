#include <bit>
#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include <Utils/Math.h>

namespace
{

    constexpr auto kNaN = std::bit_cast<float>(std::uint32_t{0x7FC00000});
    constexpr auto kInfinity = std::bit_cast<float>(std::uint32_t{0x7F800000});
    constexpr float kPi = 3.14159265358979323846f;

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

    TEST(MathTest, AbsHandlesFiniteAndNonFiniteValues)
    {
        EXPECT_EQ(Math::abs(1.0f), 1.0f);
        EXPECT_EQ(Math::abs(-1.0f), 1.0f);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(Math::abs(-0.0f)), std::uint32_t{0});
        EXPECT_EQ(Math::abs(kInfinity), kInfinity);
        EXPECT_TRUE(std::isnan(Math::abs(kNaN)));
    }

    TEST(MathTest, SincosHandlesCommonAngles)
    {
        constexpr float kTolerance = 0.00001f;
        float sine;
        float cosine;

        Math::sincos(0.0f, sine, cosine);
        EXPECT_NEAR(sine, 0.0f, kTolerance);
        EXPECT_NEAR(cosine, 1.0f, kTolerance);

        Math::sincos(kPi * 0.5f, sine, cosine);
        EXPECT_NEAR(sine, 1.0f, kTolerance);
        EXPECT_NEAR(cosine, 0.0f, kTolerance);

        Math::sincos(kPi, sine, cosine);
        EXPECT_NEAR(sine, 0.0f, kTolerance);
        EXPECT_NEAR(cosine, -1.0f, kTolerance);
    }

    TEST(MathTest, SincosHandlesNonFiniteAndTooLargeInputs)
    {
        float sine;
        float cosine;

        Math::sincos(kNaN, sine, cosine);
        EXPECT_TRUE(std::isnan(sine));
        EXPECT_TRUE(std::isnan(cosine));

        Math::sincos(kInfinity, sine, cosine);
        EXPECT_TRUE(std::isnan(sine));
        EXPECT_TRUE(std::isnan(cosine));

        Math::sincos(2.0e10f, sine, cosine);
        EXPECT_EQ(sine, 0.0f);
        EXPECT_EQ(cosine, 1.0f);
    }

    TEST(MathTest, SqrtHandlesZeroPerfectSquaresAndNegativeInput)
    {
        EXPECT_EQ(Math::sqrt(0.0f), 0.0f);
        EXPECT_EQ(Math::sqrt(1.0f), 1.0f);
        EXPECT_EQ(Math::sqrt(4.0f), 2.0f);
        EXPECT_EQ(Math::sqrt(9.0f), 3.0f);
        EXPECT_TRUE(std::isnan(Math::sqrt(-1.0f)));
    }

}
