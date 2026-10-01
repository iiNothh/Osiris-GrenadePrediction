#include <array>
#include <cmath>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <Config/ConfigFromString.h>
#include <Config/ConfigStringConversionState.h>
#include <Config/ConfigToString.h>
#include <Utils/StringParser.h>

namespace
{

struct ConfigFloatReadCase {
    std::u8string_view input;
    bool accepted;
    float expectedValue{};
    bool expectedNegativeZero{};
};

class ConfigFloatFromStringTest : public testing::TestWithParam<ConfigFloatReadCase> {
};

TEST_P(ConfigFloatFromStringTest, ParsesFloatValues) {
    const auto& param = GetParam();
    std::u8string buffer{ u8"{\"Value\":" };
    buffer += param.input;
    buffer += u8'}';

    ConfigStringConversionState conversionState;
    ConfigFromString configFromString{ buffer, conversionState };
    bool valueWasSet{};
    float value{ 42.0f };

    configFromString.beginRoot();
    configFromString.floatValue(u8"Value", [&](float parsedValue) {
        valueWasSet = true;
        value = parsedValue;
    }, [] { return 0.0f; });
    const auto readBytes = configFromString.endRoot();

    EXPECT_EQ(valueWasSet, param.accepted);
    if (param.accepted) {
        EXPECT_FLOAT_EQ(value, param.expectedValue);
        EXPECT_EQ(std::signbit(value), param.expectedNegativeZero);
        EXPECT_EQ(readBytes, buffer.size());
    } else {
        EXPECT_FLOAT_EQ(value, 42.0f);
    }
}

INSTANTIATE_TEST_SUITE_P(AllForms, ConfigFloatFromStringTest, testing::Values(
    ConfigFloatReadCase{ .input = u8"0", .accepted = true, .expectedValue = 0.0f },
    ConfigFloatReadCase{ .input = u8"-0.0e+12", .accepted = true, .expectedValue = -0.0f, .expectedNegativeZero = true },
    ConfigFloatReadCase{ .input = u8"1e+2", .accepted = true, .expectedValue = 100.0f },
    ConfigFloatReadCase{ .input = u8"1E-2", .accepted = true, .expectedValue = 0.01f },
    ConfigFloatReadCase{ .input = u8"1.234567885", .accepted = true, .expectedValue = 1.2345678f },
    ConfigFloatReadCase{ .input = u8"1.234567895", .accepted = true, .expectedValue = 1.2345679f },
    ConfigFloatReadCase{ .input = u8"1e-1001", .accepted = true, .expectedValue = 0.0f },
    ConfigFloatReadCase{ .input = u8"-1e-1001", .accepted = true, .expectedValue = -0.0f, .expectedNegativeZero = true },
    ConfigFloatReadCase{ .input = u8"1.", .accepted = false },
    ConfigFloatReadCase{ .input = u8"1e", .accepted = false },
    ConfigFloatReadCase{ .input = u8"1e+", .accepted = false },
    ConfigFloatReadCase{ .input = u8"1x", .accepted = false },
    ConfigFloatReadCase{ .input = u8"1e39", .accepted = false },
    ConfigFloatReadCase{ .input = u8"1e1001", .accepted = false }
));

class FloatParserParityTest : public testing::TestWithParam<std::u8string_view> {
};

TEST_P(FloatParserParityTest, MatchesConfigAndCommandFloatGrammar) {
    const auto input = GetParam();
    StringParser stringParser{ reinterpret_cast<const char*>(input.data()) };
    float stringParserValue{};
    const auto stringParserAccepted = stringParser.parseFloat(stringParserValue);

    std::u8string buffer{ u8"{\"Value\":" };
    buffer += input;
    buffer += u8'}';
    ConfigStringConversionState conversionState;
    ConfigFromString configFromString{ buffer, conversionState };
    bool configAccepted{};
    float configValue{};

    configFromString.beginRoot();
    configFromString.floatValue(u8"Value", [&](float parsedValue) {
        configAccepted = true;
        configValue = parsedValue;
    }, [] { return 0.0f; });
    (void)configFromString.endRoot();

    EXPECT_EQ(configAccepted, stringParserAccepted);
    if (configAccepted) {
        EXPECT_FLOAT_EQ(configValue, stringParserValue);
        EXPECT_EQ(std::signbit(configValue), std::signbit(stringParserValue));
    }
}

INSTANTIATE_TEST_SUITE_P(SharedNumericGrammar, FloatParserParityTest, testing::Values(
    u8"0",
    u8"-0.0e+12",
    u8"1.234567885",
    u8"1.234567895",
    u8"1.2345678851",
    u8"1E-2",
    u8"1e-1001",
    u8"1e39",
    u8"1e1001",
    u8"1e+",
    u8"1.0x"
));

struct ConfigFloatWriteCase {
    float value;
    std::u8string_view expectedOutput;
};

class ConfigFloatToStringTest : public testing::TestWithParam<ConfigFloatWriteCase> {
};

TEST_P(ConfigFloatToStringTest, WritesFiniteFloatValues) {
    const auto& param = GetParam();
    std::array<char8_t, 64> buffer{};
    ConfigStringConversionState conversionState;
    ConfigToString configToString{ buffer, conversionState };

    configToString.beginRoot();
    configToString.floatValue(u8"Value", [](float) {}, [&] { return param.value; });
    const auto writtenBytes = configToString.endRoot();

    EXPECT_EQ((std::u8string_view{ buffer.data(), writtenBytes }), param.expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(CurrentFormatting, ConfigFloatToStringTest, testing::Values(
    ConfigFloatWriteCase{ .value = 0.0f, .expectedOutput = u8"{\"Value\":0}" },
    ConfigFloatWriteCase{ .value = -0.0f, .expectedOutput = u8"{\"Value\":0}" },
    ConfigFloatWriteCase{ .value = 1.25f, .expectedOutput = u8"{\"Value\":1.25e0}" },
    ConfigFloatWriteCase{ .value = -1.25f, .expectedOutput = u8"{\"Value\":-1.25e0}" },
    ConfigFloatWriteCase{ .value = 0.01f, .expectedOutput = u8"{\"Value\":9.99999905e-3}" }
));

TEST(ConfigFloatToStringTest, WritesValueWhenBufferHasExactOutputCapacity)
{
    std::array<char8_t, 13> buffer{};
    ConfigStringConversionState conversionState;
    ConfigToString configToString{ buffer, conversionState };

    configToString.beginRoot();
    configToString.floatValue(u8"Value", [](float) {}, [] { return 1.0f; });
    const auto writtenBytes = configToString.endRoot();

    EXPECT_EQ(writtenBytes, buffer.size());
    EXPECT_EQ((std::u8string_view{ buffer.data(), writtenBytes }), u8"{\"Value\":1e0}");
}

}
