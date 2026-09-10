#include <limits>

#include <gtest/gtest.h>

#include <GameClient/EngineTrace/TraceResult.h>

namespace
{

TEST(TraceResultTest, ValidatesDecodedOutputSemantics)
{
    EXPECT_TRUE((TraceResult{1.0f, {1.0f, 2.0f, 3.0f}, {}}.isValid()));
    EXPECT_TRUE((TraceResult{0.5f, {1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 1.0f}}.isValid()));
    EXPECT_FALSE((TraceResult{-0.1f, {}, {0.0f, 0.0f, 1.0f}}.isValid()));
    EXPECT_FALSE((TraceResult{1.1f, {}, {0.0f, 0.0f, 1.0f}}.isValid()));
    EXPECT_FALSE((TraceResult{1.0f, {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}, {}}.isValid()));
    EXPECT_FALSE((TraceResult{1.0f, {}, {std::numeric_limits<float>::infinity(), 0.0f, 0.0f}}.isValid()));
    EXPECT_FALSE((TraceResult{0.5f, {}, {}}.isValid()));
}

}
