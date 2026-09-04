#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include <CS2/EngineTrace/EngineTraceTypes.h>
#include <GameClient/EngineTrace/NativePipTrace.h>

namespace
{

TEST(EngineTraceOutputValidationTest, RejectsRawHandleOffsetsOverlappingRequiredOutputs)
{
    constexpr auto endPositionOffset = 0x10;
    constexpr auto normalOffset = 0x20;
    constexpr auto fractionOffset = 0x30;

    EXPECT_TRUE(cs2::engine_trace::isValidRawEntityHandleOffset(0xBC, endPositionOffset, normalOffset, fractionOffset));
    EXPECT_FALSE(cs2::engine_trace::isValidRawEntityHandleOffset(0x18, endPositionOffset, normalOffset, fractionOffset));
    EXPECT_FALSE(cs2::engine_trace::isValidRawEntityHandleOffset(fractionOffset, endPositionOffset, normalOffset, fractionOffset));
}

TEST(EngineTraceDescriptorTest, UsesFixedHullLayout)
{
    const cs2::engine_trace::HullTraceDescriptor descriptor{
        .mins = {-2.0f, -2.0f, -2.0f},
        .maxs = {2.0f, 2.0f, 2.0f}
    };
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(descriptor)>>(descriptor);
    constexpr std::array<std::byte, sizeof(descriptor)> expected{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0xC0},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0xC0},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0xC0},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x40},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x40},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x40},
        std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{},
        std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{},
        std::byte{0x02}, std::byte{}, std::byte{}, std::byte{},
        std::byte{}, std::byte{}, std::byte{}, std::byte{}
    };

    EXPECT_EQ(bytes, expected);
}

constexpr engine_trace::native_pip::FilterOverlayLayout kCanonicalFilterOverlayLayout{
    .interactsExcludeOffset = 0x10,
    .interactsAsOffset = 0x18,
    .flagsOffset = 0x39,
    .candidateCollectionModeOffset = 0x40
};

constexpr engine_trace::native_pip::FilterOverlayLayout kAlternateFilterOverlayLayout{
    .interactsExcludeOffset = 0x20,
    .interactsAsOffset = 0x28,
    .flagsOffset = 0x10,
    .candidateCollectionModeOffset = 0x11
};

void mockInitFilter(cs2::engine_trace::TraceFilterStorage& filter) noexcept
{
    cs2::engine_trace::writeFilterValue(filter, 0x08, engine_trace::native_pip::kFirstInteraction);
    cs2::engine_trace::writeFilterValue(filter, 0x34, std::uint16_t{0xFFFF});
    filter.storage[0x36] = std::byte{};
    filter.storage[0x37] = std::byte{engine_trace::native_pip::kQueryByte};
    filter.storage[0x38] = std::byte{engine_trace::native_pip::kCollisionGroup};
    filter.storage[kAlternateFilterOverlayLayout.flagsOffset] = std::byte{0xA4};
    filter.storage[kAlternateFilterOverlayLayout.candidateCollectionModeOffset] = std::byte{0xA5};
}

TEST(EngineTraceNativePipFilterLayoutTest, AcceptsCanonicalAndAlternateLayouts)
{
    EXPECT_TRUE(engine_trace::native_pip::hasValidFilterOverlayLayout(kCanonicalFilterOverlayLayout));
    EXPECT_TRUE(engine_trace::native_pip::hasValidFilterOverlayLayout(kAlternateFilterOverlayLayout));
}

TEST(EngineTraceNativePipFilterLayoutTest, RejectsZeroAndVtableRegionOffsets)
{
    auto layout = kCanonicalFilterOverlayLayout;
    layout.interactsExcludeOffset = 0;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.interactsAsOffset = 0;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.flagsOffset = 0;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.candidateCollectionModeOffset = 0;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));

    layout = kCanonicalFilterOverlayLayout;
    layout.interactsExcludeOffset = sizeof(void*) - 1;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.interactsAsOffset = sizeof(void*) - 1;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.flagsOffset = sizeof(void*) - 1;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.candidateCollectionModeOffset = sizeof(void*) - 1;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
}

TEST(EngineTraceNativePipFilterLayoutTest, RejectsOutOfRangeAndMisalignedQwordOffsets)
{
    auto layout = kCanonicalFilterOverlayLayout;
    layout.interactsExcludeOffset = cs2::engine_trace::kFilterCapacity;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.interactsAsOffset = cs2::engine_trace::kFilterCapacity;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.flagsOffset = cs2::engine_trace::kFilterCapacity;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.candidateCollectionModeOffset = cs2::engine_trace::kFilterCapacity;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));

    layout = kCanonicalFilterOverlayLayout;
    layout.interactsExcludeOffset = 0x11;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.interactsAsOffset = 0x19;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
}

TEST(EngineTraceNativePipFilterLayoutTest, RejectsEveryPairOfOverlappingFields)
{
    auto layout = kCanonicalFilterOverlayLayout;
    layout.interactsAsOffset = layout.interactsExcludeOffset;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.flagsOffset = layout.interactsExcludeOffset;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.candidateCollectionModeOffset = layout.interactsExcludeOffset;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.flagsOffset = layout.interactsAsOffset;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.candidateCollectionModeOffset = layout.interactsAsOffset;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.candidateCollectionModeOffset = layout.flagsOffset;
    EXPECT_FALSE(engine_trace::native_pip::hasValidFilterOverlayLayout(layout));
}

TEST(EngineTraceNativePipFilterTest, RejectsInvalidLayoutWithoutWriting)
{
    cs2::engine_trace::TraceFilterStorage filter;
    for (auto& byte : filter.storage)
        byte = std::byte{0xA5};
    const auto before = filter;
    auto invalidLayout = kCanonicalFilterOverlayLayout;
    invalidLayout.interactsExcludeOffset = 0;

    EXPECT_FALSE(engine_trace::native_pip::applyFilterOverlay(filter, invalidLayout));
    for (std::size_t i{}; i < cs2::engine_trace::kFilterCapacity; ++i)
        EXPECT_EQ(filter.storage[i], before.storage[i]);
}

TEST(EngineTraceNativePipFilterTest, AppliesOnlyAtResolvedOffsetsAndPreservesFlagBits)
{
    cs2::engine_trace::TraceFilterStorage filter;
    for (auto& byte : filter.storage)
        byte = std::byte{0xA5};
    mockInitFilter(filter);
    const auto afterInitFilter = filter;

    ASSERT_TRUE(engine_trace::native_pip::applyFilterOverlay(filter, kAlternateFilterOverlayLayout));

    for (std::size_t i{}; i < cs2::engine_trace::kFilterCapacity; ++i) {
        if (!engine_trace::native_pip::isFilterOverlayWriteOffset(kAlternateFilterOverlayLayout, i))
            EXPECT_EQ(filter.storage[i], afterInitFilter.storage[i]);
    }
    EXPECT_EQ(cs2::engine_trace::readFilterValue<std::uint64_t>(filter, 0x08), cs2::engine_trace::readFilterValue<std::uint64_t>(afterInitFilter, 0x08));
    EXPECT_EQ(cs2::engine_trace::readFilterValue<std::uint64_t>(filter, kAlternateFilterOverlayLayout.interactsExcludeOffset), engine_trace::native_pip::kFilterInteractionMask);
    EXPECT_EQ(cs2::engine_trace::readFilterValue<std::uint64_t>(filter, kAlternateFilterOverlayLayout.interactsAsOffset), engine_trace::native_pip::kFilterObjectMask);
    EXPECT_EQ(cs2::engine_trace::readFilterValue<std::uint16_t>(filter, 0x34), cs2::engine_trace::readFilterValue<std::uint16_t>(afterInitFilter, 0x34));
    EXPECT_EQ(filter.storage[0x36], afterInitFilter.storage[0x36]);
    EXPECT_EQ(filter.storage[0x37], afterInitFilter.storage[0x37]);
    EXPECT_EQ(filter.storage[0x38], afterInitFilter.storage[0x38]);
    EXPECT_EQ(filter.storage[kAlternateFilterOverlayLayout.flagsOffset], std::byte{0xA6});
    EXPECT_EQ(filter.storage[kAlternateFilterOverlayLayout.candidateCollectionModeOffset], std::byte{0x01});

    const auto afterFirstOverlay = filter;
    ASSERT_TRUE(engine_trace::native_pip::applyFilterOverlay(filter, kAlternateFilterOverlayLayout));
    for (std::size_t i{}; i < cs2::engine_trace::kFilterCapacity; ++i)
        EXPECT_EQ(filter.storage[i], afterFirstOverlay.storage[i]);
}

}
