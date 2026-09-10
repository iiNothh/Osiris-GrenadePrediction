#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include <GameClient/EngineTrace/GrenadeTrace.h>

namespace
{

constexpr engine_trace::grenade::FilterOverlayLayout kCanonicalFilterOverlayLayout{
    .interactsExcludeOffset = 0x10,
    .interactsAsOffset = 0x18,
    .flagsOffset = 0x39,
    .candidateCollectionModeOffset = 0x40
};

constexpr engine_trace::grenade::FilterOverlayLayout kAlternateFilterOverlayLayout{
    .interactsExcludeOffset = 0x20,
    .interactsAsOffset = 0x28,
    .flagsOffset = 0x10,
    .candidateCollectionModeOffset = 0x11
};

void mockConstructFilter(cs2::CTraceFilter& filter) noexcept
{
    filter.writeValue(0x08, engine_trace::grenade::kFirstInteraction);
    filter.writeValue(0x34, std::uint16_t{0xFFFF});
    filter.storage[0x36] = std::byte{};
    filter.storage[0x37] = std::byte{static_cast<std::uint8_t>(engine_trace::grenade::kQueryFlags)};
    filter.storage[0x38] = std::byte{static_cast<std::uint8_t>(engine_trace::grenade::kCollisionGroup)};
    filter.storage[kAlternateFilterOverlayLayout.flagsOffset] = std::byte{0xA4};
    filter.storage[kAlternateFilterOverlayLayout.candidateCollectionModeOffset] = std::byte{0xA5};
}

TEST(EngineTraceGrenadeFilterLayoutTest, AcceptsCanonicalAndAlternateLayouts)
{
    EXPECT_TRUE(engine_trace::grenade::hasValidFilterOverlayLayout(kCanonicalFilterOverlayLayout));
    EXPECT_TRUE(engine_trace::grenade::hasValidFilterOverlayLayout(kAlternateFilterOverlayLayout));
}

TEST(EngineTraceGrenadeFilterTest, DefinesObservedFilterMasks)
{
    EXPECT_EQ(static_cast<std::uint64_t>(engine_trace::grenade::kFirstInteraction), 0x0000000200003001ull);
    EXPECT_EQ(static_cast<std::uint64_t>(engine_trace::grenade::kFilterInteractionMask), 0x00040200ull);
    EXPECT_EQ(static_cast<std::uint64_t>(engine_trace::grenade::kFilterObjectMask), 0x0000008000020001ull);
}

TEST(EngineTraceGrenadeFilterLayoutTest, RejectsZeroAndVtableRegionOffsets)
{
    auto layout = kCanonicalFilterOverlayLayout;
    layout.interactsExcludeOffset = 0;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.interactsAsOffset = 0;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.flagsOffset = 0;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.candidateCollectionModeOffset = 0;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));

    layout = kCanonicalFilterOverlayLayout;
    layout.interactsExcludeOffset = sizeof(void*) - 1;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.interactsAsOffset = sizeof(void*) - 1;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.flagsOffset = sizeof(void*) - 1;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.candidateCollectionModeOffset = sizeof(void*) - 1;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
}

TEST(EngineTraceGrenadeFilterLayoutTest, RejectsOutOfRangeAndMisalignedQwordOffsets)
{
    auto layout = kCanonicalFilterOverlayLayout;
    layout.interactsExcludeOffset = cs2::engine_trace::kCTraceFilterCapacity;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.interactsAsOffset = cs2::engine_trace::kCTraceFilterCapacity;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.flagsOffset = cs2::engine_trace::kCTraceFilterCapacity;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.candidateCollectionModeOffset = cs2::engine_trace::kCTraceFilterCapacity;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));

    layout = kCanonicalFilterOverlayLayout;
    layout.interactsExcludeOffset = 0x11;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.interactsAsOffset = 0x19;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
}

TEST(EngineTraceGrenadeFilterLayoutTest, RejectsEveryPairOfOverlappingFields)
{
    auto layout = kCanonicalFilterOverlayLayout;
    layout.interactsAsOffset = layout.interactsExcludeOffset;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.flagsOffset = layout.interactsExcludeOffset;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.candidateCollectionModeOffset = layout.interactsExcludeOffset;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.flagsOffset = layout.interactsAsOffset;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.candidateCollectionModeOffset = layout.interactsAsOffset;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
    layout = kCanonicalFilterOverlayLayout;
    layout.candidateCollectionModeOffset = layout.flagsOffset;
    EXPECT_FALSE(engine_trace::grenade::hasValidFilterOverlayLayout(layout));
}

TEST(EngineTraceGrenadeFilterTest, RejectsInvalidLayoutWithoutWriting)
{
    cs2::CTraceFilter filter;
    for (auto& byte : filter.storage)
        byte = std::byte{0xA5};
    const auto before = filter;
    auto invalidLayout = kCanonicalFilterOverlayLayout;
    invalidLayout.interactsExcludeOffset = 0;

    EXPECT_FALSE(engine_trace::grenade::applyFilterOverlay(filter, invalidLayout));
    for (std::size_t i{}; i < cs2::engine_trace::kCTraceFilterCapacity; ++i)
        EXPECT_EQ(filter.storage[i], before.storage[i]);
}

TEST(EngineTraceGrenadeFilterTest, AppliesOnlyAtResolvedOffsetsAndPreservesFlagBits)
{
    cs2::CTraceFilter filter;
    for (auto& byte : filter.storage)
        byte = std::byte{0xA5};
    mockConstructFilter(filter);
    const auto afterFilterConstruction = filter;

    ASSERT_TRUE(engine_trace::grenade::applyFilterOverlay(filter, kAlternateFilterOverlayLayout));

    for (std::size_t i{}; i < cs2::engine_trace::kCTraceFilterCapacity; ++i) {
        if (!engine_trace::grenade::isFilterOverlayWriteOffset(kAlternateFilterOverlayLayout, i))
            EXPECT_EQ(filter.storage[i], afterFilterConstruction.storage[i]);
    }
    EXPECT_EQ(filter.readValue<cs2::engine_trace::InteractionLayer>(0x08), afterFilterConstruction.readValue<cs2::engine_trace::InteractionLayer>(0x08));
    EXPECT_EQ(filter.readValue<cs2::engine_trace::InteractionLayer>(kAlternateFilterOverlayLayout.interactsExcludeOffset), engine_trace::grenade::kFilterInteractionMask);
    EXPECT_EQ(filter.readValue<cs2::engine_trace::InteractionLayer>(kAlternateFilterOverlayLayout.interactsAsOffset), engine_trace::grenade::kFilterObjectMask);
    EXPECT_EQ(filter.readValue<std::uint16_t>(0x34), afterFilterConstruction.readValue<std::uint16_t>(0x34));
    EXPECT_EQ(filter.storage[0x36], afterFilterConstruction.storage[0x36]);
    EXPECT_EQ(filter.storage[0x37], afterFilterConstruction.storage[0x37]);
    EXPECT_EQ(filter.storage[0x38], afterFilterConstruction.storage[0x38]);
    EXPECT_EQ(filter.storage[kAlternateFilterOverlayLayout.flagsOffset], std::byte{0xA6});
    EXPECT_EQ(filter.storage[kAlternateFilterOverlayLayout.candidateCollectionModeOffset], std::byte{0x01});

    const auto afterFirstOverlay = filter;
    ASSERT_TRUE(engine_trace::grenade::applyFilterOverlay(filter, kAlternateFilterOverlayLayout));
    for (std::size_t i{}; i < cs2::engine_trace::kCTraceFilterCapacity; ++i)
        EXPECT_EQ(filter.storage[i], afterFirstOverlay.storage[i]);
}

}
