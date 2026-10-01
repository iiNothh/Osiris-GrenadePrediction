#include <array>
#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include <UI/Panorama/Tabs/VisualsTab/FloatSlider.h>

namespace
{

struct TestHookContext;
using TestFloatSlider = FloatSlider<TestHookContext>;

struct FloatSliderFormatCase {
    float value;
    std::uint32_t decimalPrecision;
    const char* expectedOutput;
};

class FloatSliderFormatTest : public testing::TestWithParam<FloatSliderFormatCase> {
};

TEST_P(FloatSliderFormatTest, NormalizesToFixedPrecision) {
    const auto& param = GetParam();
    std::array<char, 21> output{};

    EXPECT_TRUE(TestFloatSlider::formatFixed(param.value, param.decimalPrecision, output.data(), output.size()));
    EXPECT_STREQ(output.data(), param.expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(RoundingBoundaries, FloatSliderFormatTest, testing::Values(
    FloatSliderFormatCase{ .value = 1.234f, .decimalPrecision = 2, .expectedOutput = "1.23" },
    FloatSliderFormatCase{ .value = 1.235f, .decimalPrecision = 2, .expectedOutput = "1.24" },
    FloatSliderFormatCase{ .value = -1.235f, .decimalPrecision = 2, .expectedOutput = "-1.24" },
    FloatSliderFormatCase{ .value = -0.004f, .decimalPrecision = 2, .expectedOutput = "0.00" },
    FloatSliderFormatCase{ .value = -0.005f, .decimalPrecision = 2, .expectedOutput = "-0.01" },
    FloatSliderFormatCase{ .value = 1.5f, .decimalPrecision = 0, .expectedOutput = "2" },
    FloatSliderFormatCase{ .value = -1.5f, .decimalPrecision = 0, .expectedOutput = "-2" }
));

TEST(FloatSliderFormatTest, RequiresAndAcceptsTheExactMinimumOutputCapacity)
{
    std::array<char, 21> output{};

    EXPECT_FALSE(TestFloatSlider::formatFixed(1.25f, 2, output.data(), output.size() - 1));
    EXPECT_TRUE(TestFloatSlider::formatFixed(1.25f, 2, output.data(), output.size()));
    EXPECT_STREQ(output.data(), "1.25");
}

TEST(FloatSliderFormatTest, RejectsNonFiniteAndOutOfRangeValues)
{
    std::array<char, 21> output{};

    EXPECT_FALSE(TestFloatSlider::formatFixed(std::numeric_limits<float>::infinity(), 2, output.data(), output.size()));
    EXPECT_FALSE(TestFloatSlider::formatFixed(-std::numeric_limits<float>::infinity(), 2, output.data(), output.size()));
    EXPECT_FALSE(TestFloatSlider::formatFixed(std::numeric_limits<float>::quiet_NaN(), 2, output.data(), output.size()));
    EXPECT_FALSE(TestFloatSlider::formatFixed(2'147'484.0f, 2, output.data(), output.size()));
}

}
