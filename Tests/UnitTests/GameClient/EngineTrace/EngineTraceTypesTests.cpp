#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

#include <gtest/gtest.h>

#include <GameClient/EngineTrace/EngineTraceTypes.h>

namespace
{

TEST(EngineTraceOutputValidationTest, RejectsRawHandleOffsetsOverlappingRequiredOutputs)
{
    constexpr auto endPositionOffset = 0x10;
    constexpr auto normalOffset = 0x20;
    constexpr auto fractionOffset = 0x30;

    EXPECT_TRUE(engine_trace::isValidRawEntityHandleOffset(0xBC, endPositionOffset, normalOffset, fractionOffset));
    EXPECT_FALSE(engine_trace::isValidRawEntityHandleOffset(0x18, endPositionOffset, normalOffset, fractionOffset));
    EXPECT_FALSE(engine_trace::isValidRawEntityHandleOffset(fractionOffset, endPositionOffset, normalOffset, fractionOffset));
}

TEST(EngineTraceDescriptorTest, UsesFixedGrenadeHullLayout)
{
    const engine_trace::FixedGrenadeHullTraceDescriptor descriptor{};
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

TEST(EngineTraceNativePipFilterTest, OverlayWritesOnlyTheWhitelistedBytes)
{
    engine_trace::FilterStorage filter;
    for (auto& byte : filter.storage)
        byte = std::byte{0xA5};
    const auto before = std::bit_cast<std::array<std::byte, engine_trace::kFilterCapacity>>(filter);

    engine_trace::applyNativePipFilterOverlay(filter);

    for (std::size_t i{}; i < engine_trace::kFilterCapacity; ++i) {
        if (!engine_trace::isNativePipFilterWriteWhitelistedOffset(i))
            EXPECT_EQ(filter.storage[i], before[i]);
    }
    for (std::size_t i{0x3A}; i <= 0x3F; ++i) {
        EXPECT_FALSE(engine_trace::isNativePipFilterWriteWhitelistedOffset(i));
        EXPECT_EQ(filter.storage[i], before[i]);
    }
    EXPECT_EQ(engine_trace::readFilterValue<std::uint64_t>(filter, 0x08), engine_trace::kNativePipFirstInteraction);
    EXPECT_EQ(engine_trace::readFilterValue<std::uint64_t>(filter, 0x10), engine_trace::kNativePipFilterInteractionMask);
    EXPECT_EQ(engine_trace::readFilterValue<std::uint64_t>(filter, 0x18), engine_trace::kNativePipFilterObjectMask);
    EXPECT_EQ(engine_trace::readFilterValue<std::uint16_t>(filter, 0x34), 0xFFFF);
    EXPECT_EQ(filter.storage[0x36], std::byte{});
    EXPECT_EQ(filter.storage[0x37], std::byte{0x0F});
    EXPECT_EQ(filter.storage[0x38], std::byte{0x10});
    EXPECT_EQ(filter.storage[0x39], std::byte{0x4B});
    EXPECT_EQ(filter.storage[0x40], std::byte{0x01});
}

TEST(EngineTraceNativePipFilterTest, RequiresReadableBaseVtableAndExpectedCallback)
{
    std::array<std::byte, 3> callback{std::byte{0xB0}, std::byte{0x01}, std::byte{0xC3}};
    void* vtable[2]{nullptr, callback.data()};
    const MemorySection callbackCode{std::span{callback}};
    const MemorySection vtableSection{std::span{reinterpret_cast<const std::byte*>(vtable), sizeof(vtable)}};
    const MemorySection incompleteVtableSection{std::span{reinterpret_cast<const std::byte*>(vtable), sizeof(void*)}};

    const auto* const callbackAddress = engine_trace::nativePipBaseFilterCallback(vtableSection, vtable);
    EXPECT_EQ(callbackAddress, callback.data());
    EXPECT_TRUE(engine_trace::hasExpectedNativePipBaseFilterCallback(callbackCode, callbackAddress));
    EXPECT_EQ(engine_trace::nativePipBaseFilterCallback(incompleteVtableSection, vtable), nullptr);
    EXPECT_EQ(engine_trace::nativePipBaseFilterCallback(MemorySection{}, vtable), nullptr);
    callback[1] = std::byte{};
    EXPECT_FALSE(engine_trace::hasExpectedNativePipBaseFilterCallback(callbackCode, callbackAddress));
}

TEST(EngineTraceNativePipFilterTest, RequiresEveryResolvedProfileMarker)
{
    std::byte marker{};
    void* vtable[2]{};

    EXPECT_TRUE(engine_trace::hasNativePipProfileMarkers(vtable, &marker, &marker, &marker, &marker, &marker, &marker));
    EXPECT_FALSE(engine_trace::hasNativePipProfileMarkers(nullptr, &marker, &marker, &marker, &marker, &marker, &marker));
    EXPECT_FALSE(engine_trace::hasNativePipProfileMarkers(vtable, nullptr, &marker, &marker, &marker, &marker, &marker));
    EXPECT_FALSE(engine_trace::hasNativePipProfileMarkers(vtable, &marker, nullptr, &marker, &marker, &marker, &marker));
    EXPECT_FALSE(engine_trace::hasNativePipProfileMarkers(vtable, &marker, &marker, nullptr, &marker, &marker, &marker));
    EXPECT_FALSE(engine_trace::hasNativePipProfileMarkers(vtable, &marker, &marker, &marker, nullptr, &marker, &marker));
    EXPECT_FALSE(engine_trace::hasNativePipProfileMarkers(vtable, &marker, &marker, &marker, &marker, nullptr, &marker));
    EXPECT_FALSE(engine_trace::hasNativePipProfileMarkers(vtable, &marker, &marker, &marker, &marker, &marker, nullptr));
}

TEST(EngineTraceNativePipFilterTest, RejectsUnexpectedProfileStructuralDeltas)
{
    EXPECT_TRUE(engine_trace::hasCurrentNativePipProfileStructure(0x1000, 0x25C3, 0x1210, 0x2000));
    EXPECT_FALSE(engine_trace::hasCurrentNativePipProfileStructure(0x1000, 0x25C3, 0x120F, 0x2000));
    EXPECT_FALSE(engine_trace::hasCurrentNativePipProfileStructure(0x1000, 0x25C2, 0x1210, 0x2000));
    EXPECT_FALSE(engine_trace::hasCurrentNativePipProfileStructure(0x1210, 0x25C3, 0x1000, 0x2000));
}

}
