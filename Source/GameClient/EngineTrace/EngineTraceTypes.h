#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <CS2/Classes/Vector.h>

namespace engine_trace {

static_assert(std::endian::native == std::endian::little);

constexpr std::size_t kDescriptorCapacity{0x30};
constexpr std::size_t kFilterCapacity{72};
constexpr std::size_t kOutputCapacity{0xC0};
constexpr std::int32_t kWorldEntityHandle{0x8000};

struct TraceFilterExcludedEntities {
    void* first{};
    void* second{};
};

struct alignas(8) FixedGrenadeHullTraceDescriptor {
    cs2::Vector mins{-2.0f, -2.0f, -2.0f};
    cs2::Vector maxs{2.0f, 2.0f, 2.0f};
    std::byte zeroesBeforeType[0x10]{};
    std::uint32_t type{2};
    std::byte trailingZeroes[0x4]{};
};

struct alignas(8) FilterStorage {
    std::byte storage[kFilterCapacity]{};
};

struct alignas(16) OutputStorage {
    std::byte storage[kOutputCapacity]{};
};

static_assert(sizeof(cs2::Vector) == 0xC);
static_assert(alignof(cs2::Vector) == alignof(float));
static_assert(std::is_standard_layout_v<cs2::Vector>);
static_assert(offsetof(cs2::Vector, x) == 0x0);
static_assert(offsetof(cs2::Vector, y) == 0x4);
static_assert(offsetof(cs2::Vector, z) == 0x8);
static_assert(std::is_standard_layout_v<FixedGrenadeHullTraceDescriptor>);
static_assert(std::is_trivially_copyable_v<FixedGrenadeHullTraceDescriptor>);
static_assert(sizeof(FixedGrenadeHullTraceDescriptor) == kDescriptorCapacity);
static_assert(alignof(FixedGrenadeHullTraceDescriptor) == 8);
static_assert(offsetof(FixedGrenadeHullTraceDescriptor, mins) == 0x0);
static_assert(offsetof(FixedGrenadeHullTraceDescriptor, maxs) == 0xC);
static_assert(offsetof(FixedGrenadeHullTraceDescriptor, zeroesBeforeType) == 0x18);
static_assert(offsetof(FixedGrenadeHullTraceDescriptor, type) == 0x28);
static_assert(offsetof(FixedGrenadeHullTraceDescriptor, trailingZeroes) == 0x2C);
static_assert(sizeof(FilterStorage) == kFilterCapacity && alignof(FilterStorage) == 8);
static_assert(sizeof(OutputStorage) == kOutputCapacity && alignof(OutputStorage) == 16);

[[nodiscard]] constexpr bool isValidOutputOffset(std::int32_t offset, std::size_t fieldSize) noexcept
{
    return offset > 0
        && static_cast<std::size_t>(offset) % alignof(float) == 0
        && static_cast<std::size_t>(offset) <= kOutputCapacity
        && fieldSize <= kOutputCapacity - static_cast<std::size_t>(offset);
}

[[nodiscard]] constexpr bool doRegionsOverlap(std::size_t firstOffset, std::size_t firstSize, std::size_t secondOffset, std::size_t secondSize) noexcept
{
    return firstOffset < secondOffset + secondSize
        && secondOffset < firstOffset + firstSize;
}

[[nodiscard]] constexpr bool areValidOutputOffsets(std::int32_t endPositionOffset, std::int32_t normalOffset, std::int32_t fractionOffset) noexcept
{
    return isValidOutputOffset(endPositionOffset, sizeof(cs2::Vector))
        && isValidOutputOffset(normalOffset, sizeof(cs2::Vector))
        && isValidOutputOffset(fractionOffset, sizeof(float))
        && !doRegionsOverlap(static_cast<std::size_t>(endPositionOffset), sizeof(cs2::Vector), static_cast<std::size_t>(normalOffset), sizeof(cs2::Vector))
        && !doRegionsOverlap(static_cast<std::size_t>(endPositionOffset), sizeof(cs2::Vector), static_cast<std::size_t>(fractionOffset), sizeof(float))
        && !doRegionsOverlap(static_cast<std::size_t>(normalOffset), sizeof(cs2::Vector), static_cast<std::size_t>(fractionOffset), sizeof(float));
}

[[nodiscard]] constexpr bool isValidRawEntityHandleOffset(std::int32_t rawEntityHandleOffset, std::int32_t endPositionOffset,
    std::int32_t normalOffset, std::int32_t fractionOffset) noexcept
{
    return isValidOutputOffset(rawEntityHandleOffset, sizeof(std::int32_t))
        && !doRegionsOverlap(static_cast<std::size_t>(rawEntityHandleOffset), sizeof(std::int32_t), static_cast<std::size_t>(endPositionOffset), sizeof(cs2::Vector))
        && !doRegionsOverlap(static_cast<std::size_t>(rawEntityHandleOffset), sizeof(std::int32_t), static_cast<std::size_t>(normalOffset), sizeof(cs2::Vector))
        && !doRegionsOverlap(static_cast<std::size_t>(rawEntityHandleOffset), sizeof(std::int32_t), static_cast<std::size_t>(fractionOffset), sizeof(float));
}

[[nodiscard]] constexpr bool isFinite(float value) noexcept
{
    return (std::bit_cast<std::uint32_t>(value) & 0x7F800000u) != 0x7F800000u;
}

[[nodiscard]] constexpr bool isFinite(cs2::Vector value) noexcept
{
    return isFinite(value.x) && isFinite(value.y) && isFinite(value.z);
}

[[nodiscard]] constexpr bool hasUsableNormal(cs2::Vector normal) noexcept
{
    return isFinite(normal) && (normal.x != 0.0f || normal.y != 0.0f || normal.z != 0.0f);
}

}
