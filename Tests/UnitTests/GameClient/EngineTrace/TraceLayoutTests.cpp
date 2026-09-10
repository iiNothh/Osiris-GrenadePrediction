#include <cstdint>

#include <gtest/gtest.h>

#include <GameClient/EngineTrace/TraceLayout.h>

namespace
{

TEST(EngineTraceLayoutTest, RecognizesOverlappingAndNonOverlappingRegions)
{
    EXPECT_FALSE(engine_trace::regionsOverlap(0x10, 0x08, 0x18, 0x04));
    EXPECT_FALSE(engine_trace::regionsOverlap(0x10, 0x08, 0x20, 0x04));
    EXPECT_TRUE(engine_trace::regionsOverlap(0x10, 0x08, 0x17, 0x04));
    EXPECT_TRUE(engine_trace::regionsOverlap(0x10, 0x10, 0x18, 0x04));
}

TEST(EngineTraceOutputValidationTest, RejectsRawHandleOffsetsOverlappingRequiredOutputs)
{
    constexpr auto endPositionOffset = 0x10;
    constexpr auto normalOffset = 0x20;
    constexpr auto fractionOffset = 0x30;

    EXPECT_TRUE(engine_trace::isValidCGameTraceRawEntityHandleOffset(0xBC, endPositionOffset, normalOffset, fractionOffset));
    EXPECT_FALSE(engine_trace::isValidCGameTraceRawEntityHandleOffset(0x18, endPositionOffset, normalOffset, fractionOffset));
    EXPECT_FALSE(engine_trace::isValidCGameTraceRawEntityHandleOffset(fractionOffset, endPositionOffset, normalOffset, fractionOffset));
}

TEST(TraceOutputLayoutTest, ValidatesOptionalRawEntityHandleOffset)
{
    const engine_trace::TraceOutputLayout validLayout{
        .endPositionOffset = 0x10,
        .normalOffset = 0x20,
        .fractionOffset = 0x30,
        .rawEntityHandleOffset = 0x40
    };
    EXPECT_TRUE(validLayout.hasValidRawEntityHandleOffset());

    auto absentLayout = validLayout;
    absentLayout.rawEntityHandleOffset = {};
    EXPECT_FALSE(absentLayout.hasValidRawEntityHandleOffset());

    auto invalidLayout = validLayout;
    invalidLayout.rawEntityHandleOffset = std::int32_t{cs2::engine_trace::kCGameTraceCapacity};
    EXPECT_FALSE(invalidLayout.hasValidRawEntityHandleOffset());

    auto overlappingLayout = validLayout;
    overlappingLayout.rawEntityHandleOffset = overlappingLayout.endPositionOffset;
    EXPECT_FALSE(overlappingLayout.hasValidRawEntityHandleOffset());
}

}
