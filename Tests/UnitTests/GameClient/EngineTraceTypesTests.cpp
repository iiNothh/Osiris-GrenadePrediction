#include <gtest/gtest.h>

#include <GameClient/EngineTrace/EngineTraceTypes.h>

TEST(EngineTraceStorageTest, HasRequiredAbiStorage)
{
    EXPECT_EQ(sizeof(engine_trace::Bounds6f), 24);
    EXPECT_EQ(sizeof(engine_trace::DescriptorStorage), 0x30);
    EXPECT_EQ(alignof(engine_trace::DescriptorStorage), 8);
    EXPECT_EQ(sizeof(engine_trace::FilterStorage), 0x48);
    EXPECT_EQ(alignof(engine_trace::FilterStorage), 8);
    EXPECT_EQ(sizeof(engine_trace::OutputStorage), 0xC0);
    EXPECT_EQ(alignof(engine_trace::OutputStorage), 16);
}

TEST(EngineTraceOutputOffsetsTest, RejectsInvalidOrOverlappingOffsets)
{
    EXPECT_FALSE(engine_trace::areValidOutputOffsets(-4, 16, 32));
    EXPECT_FALSE(engine_trace::areValidOutputOffsets(0, 16, 32));
    EXPECT_FALSE(engine_trace::areValidOutputOffsets(2, 16, 32));
    EXPECT_FALSE(engine_trace::areValidOutputOffsets(16, 16, 32));
    EXPECT_FALSE(engine_trace::areValidOutputOffsets(16, 28, 32));
    EXPECT_FALSE(engine_trace::areValidOutputOffsets(16, 32, 0xC0));
    EXPECT_FALSE(engine_trace::areValidOutputOffsets(0xB8, 32, 48));
    EXPECT_TRUE(engine_trace::areValidOutputOffsets(16, 32, 48));
    EXPECT_TRUE(engine_trace::areValidOutputOffsets(0xB4, 16, 32));
}
