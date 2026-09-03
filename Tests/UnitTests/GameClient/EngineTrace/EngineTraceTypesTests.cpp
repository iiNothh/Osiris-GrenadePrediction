#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

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

TEST(EngineTraceNativePipFilterTest, OverlayWritesOnlyTheWhitelistedBytes)
{
    cs2::engine_trace::TraceFilterStorage filter;
    for (auto& byte : filter.storage)
        byte = std::byte{0xA5};
    const auto before = std::bit_cast<std::array<std::byte, cs2::engine_trace::kFilterCapacity>>(filter);

    engine_trace::native_pip::applyFilterOverlay(filter);

    for (std::size_t i{}; i < cs2::engine_trace::kFilterCapacity; ++i) {
        if (!engine_trace::native_pip::isFilterWriteWhitelistedOffset(i))
            EXPECT_EQ(filter.storage[i], before[i]);
    }
    for (std::size_t i{0x3A}; i <= 0x3F; ++i) {
        EXPECT_FALSE(engine_trace::native_pip::isFilterWriteWhitelistedOffset(i));
        EXPECT_EQ(filter.storage[i], before[i]);
    }
    EXPECT_EQ(cs2::engine_trace::readFilterValue<std::uint64_t>(filter, 0x08), engine_trace::native_pip::kFirstInteraction);
    EXPECT_EQ(cs2::engine_trace::readFilterValue<std::uint64_t>(filter, 0x10), engine_trace::native_pip::kFilterInteractionMask);
    EXPECT_EQ(cs2::engine_trace::readFilterValue<std::uint64_t>(filter, 0x18), engine_trace::native_pip::kFilterObjectMask);
    EXPECT_EQ(cs2::engine_trace::readFilterValue<std::uint16_t>(filter, 0x34), 0xFFFF);
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

    const auto* const callbackAddress = engine_trace::native_pip::baseFilterCallback(vtableSection, vtable);
    EXPECT_EQ(callbackAddress, callback.data());
    EXPECT_TRUE(engine_trace::native_pip::hasExpectedBaseFilterCallback(callbackCode, callbackAddress));
    EXPECT_EQ(engine_trace::native_pip::baseFilterCallback(incompleteVtableSection, vtable), nullptr);
    EXPECT_EQ(engine_trace::native_pip::baseFilterCallback(MemorySection{}, vtable), nullptr);
    callback[1] = std::byte{};
    EXPECT_FALSE(engine_trace::native_pip::hasExpectedBaseFilterCallback(callbackCode, callbackAddress));
}

TEST(EngineTraceNativePipFilterTest, RequiresEveryResolvedProfileMarker)
{
    std::byte marker{};
    void* vtable[2]{};

    EXPECT_TRUE(engine_trace::native_pip::hasProfileMarkers(vtable, &marker, &marker, &marker, &marker, &marker, &marker));
    EXPECT_FALSE(engine_trace::native_pip::hasProfileMarkers(nullptr, &marker, &marker, &marker, &marker, &marker, &marker));
    EXPECT_FALSE(engine_trace::native_pip::hasProfileMarkers(vtable, nullptr, &marker, &marker, &marker, &marker, &marker));
    EXPECT_FALSE(engine_trace::native_pip::hasProfileMarkers(vtable, &marker, nullptr, &marker, &marker, &marker, &marker));
    EXPECT_FALSE(engine_trace::native_pip::hasProfileMarkers(vtable, &marker, &marker, nullptr, &marker, &marker, &marker));
    EXPECT_FALSE(engine_trace::native_pip::hasProfileMarkers(vtable, &marker, &marker, &marker, nullptr, &marker, &marker));
    EXPECT_FALSE(engine_trace::native_pip::hasProfileMarkers(vtable, &marker, &marker, &marker, &marker, nullptr, &marker));
    EXPECT_FALSE(engine_trace::native_pip::hasProfileMarkers(vtable, &marker, &marker, &marker, &marker, &marker, nullptr));
}

TEST(EngineTraceNativePipFilterTest, RejectsUnexpectedProfileStructuralDeltas)
{
    EXPECT_TRUE(engine_trace::native_pip::hasCurrentProfileStructure(0x1000, 0x25C3, 0x1210, 0x2000));
    EXPECT_FALSE(engine_trace::native_pip::hasCurrentProfileStructure(0x1000, 0x25C3, 0x120F, 0x2000));
    EXPECT_FALSE(engine_trace::native_pip::hasCurrentProfileStructure(0x1000, 0x25C2, 0x1210, 0x2000));
    EXPECT_FALSE(engine_trace::native_pip::hasCurrentProfileStructure(0x1210, 0x25C3, 0x1000, 0x2000));
}

}
