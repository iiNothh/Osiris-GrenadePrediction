#include <cmath>
#include <string_view>

#include <gtest/gtest.h>

#include <Utils/StringParser.h>

namespace
{

TEST(StringParserTest, ParsesGrenadeFloatSettingFollowedByAnotherCommand)
{
    StringParser parser{ "set visuals/grenade_prediction_cache_duration/1.25 unload " };

    EXPECT_EQ(parser.getLine(' '), "set");
    EXPECT_EQ(parser.getLine('/'), "visuals");
    EXPECT_EQ(parser.getLine('/'), "grenade_prediction_cache_duration");

    float value{};
    EXPECT_TRUE(parser.parseFloat(value));
    EXPECT_FLOAT_EQ(value, 1.25f);

    parser.skipWhitespace();
    EXPECT_EQ(parser.getLine(' '), "unload");
}

struct StringParserFloatCase {
    std::string_view input;
    bool accepted;
    float expectedValue{};
    bool expectedNegativeZero{};
};

class StringParserFloatTest : public testing::TestWithParam<StringParserFloatCase> {
};

TEST_P(StringParserFloatTest, ParsesDocumentedFloatForms) {
    const auto& param = GetParam();
    StringParser parser{ param.input.data() };
    float value{ 42.0f };

    EXPECT_EQ(parser.parseFloat(value), param.accepted);
    if (param.accepted) {
        EXPECT_FLOAT_EQ(value, param.expectedValue);
        EXPECT_EQ(std::signbit(value), param.expectedNegativeZero);
    } else {
        EXPECT_FLOAT_EQ(value, 42.0f);
    }
}

INSTANTIATE_TEST_SUITE_P(AllForms, StringParserFloatTest, testing::Values(
    StringParserFloatCase{ .input = "0", .accepted = true, .expectedValue = 0.0f },
    StringParserFloatCase{ .input = "-0.0e+12", .accepted = true, .expectedValue = -0.0f, .expectedNegativeZero = true },
    StringParserFloatCase{ .input = "1e+2", .accepted = true, .expectedValue = 100.0f },
    StringParserFloatCase{ .input = "1E-2", .accepted = true, .expectedValue = 0.01f },
    StringParserFloatCase{ .input = "1.234567885", .accepted = true, .expectedValue = 1.2345678f },
    StringParserFloatCase{ .input = "1.234567895", .accepted = true, .expectedValue = 1.2345679f },
    StringParserFloatCase{ .input = "1.2345678851", .accepted = true, .expectedValue = 1.2345679f },
    StringParserFloatCase{ .input = "1e-1001", .accepted = true, .expectedValue = 0.0f },
    StringParserFloatCase{ .input = "-1e-1001", .accepted = true, .expectedValue = -0.0f, .expectedNegativeZero = true },
    StringParserFloatCase{ .input = "", .accepted = false },
    StringParserFloatCase{ .input = "-", .accepted = false },
    StringParserFloatCase{ .input = ".1", .accepted = false },
    StringParserFloatCase{ .input = "1.", .accepted = false },
    StringParserFloatCase{ .input = "1e", .accepted = false },
    StringParserFloatCase{ .input = "1e+", .accepted = false },
    StringParserFloatCase{ .input = "1e-", .accepted = false },
    StringParserFloatCase{ .input = "+1", .accepted = false },
    StringParserFloatCase{ .input = "1x", .accepted = false },
    StringParserFloatCase{ .input = "1.0x", .accepted = false },
    StringParserFloatCase{ .input = "1e2x", .accepted = false },
    StringParserFloatCase{ .input = "1e39", .accepted = false },
    StringParserFloatCase{ .input = "1e1001", .accepted = false }
));

}
