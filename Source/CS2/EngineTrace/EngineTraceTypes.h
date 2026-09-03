#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

#include <CS2/Classes/Vector.h>

namespace cs2::engine_trace {

static_assert(std::endian::native == std::endian::little);

constexpr std::size_t kDescriptorCapacity{0x30};
constexpr std::size_t kFilterCapacity{0x48};
constexpr std::size_t kOutputCapacity{0xC0};
constexpr std::int32_t kWorldEntityHandle{0x8000};

struct alignas(8) HullTraceDescriptor {
    Vector mins{};
    Vector maxs{};
    std::byte zeroesBeforeType[0x10]{};
    std::uint32_t type{2};
    std::byte trailingZeroes[0x4]{};
};

struct alignas(8) TraceFilterStorage {
    std::byte storage[kFilterCapacity]{};
};

struct alignas(16) TraceOutputStorage {
    std::byte storage[kOutputCapacity]{};
};

static_assert(sizeof(Vector) == 0xC && alignof(Vector) == alignof(float));
static_assert(sizeof(HullTraceDescriptor) == 0x30 && alignof(HullTraceDescriptor) == 8 && offsetof(HullTraceDescriptor, type) == 0x28);
static_assert(sizeof(TraceFilterStorage) == 0x48 && alignof(TraceFilterStorage) == 8);
static_assert(sizeof(TraceOutputStorage) == 0xC0 && alignof(TraceOutputStorage) == 16);

template <typename T>
[[nodiscard]] T readFilterValue(const TraceFilterStorage& filter, std::size_t offset) noexcept
{
    static_assert(sizeof(T) <= kFilterCapacity);
    if (offset > kFilterCapacity - sizeof(T))
        return {};

    std::array<std::byte, sizeof(T)> bytes{};
    for (std::size_t i = 0; i < sizeof(T); ++i)
        bytes[i] = filter.storage[offset + i];
    return std::bit_cast<T>(bytes);
}

template <typename T>
void writeFilterValue(TraceFilterStorage& filter, std::size_t offset, T value) noexcept
{
    static_assert(sizeof(T) <= kFilterCapacity);
    if (offset > kFilterCapacity - sizeof(T))
        return;

    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    for (std::size_t i = 0; i < sizeof(T); ++i)
        filter.storage[offset + i] = bytes[i];
}

template <typename T>
[[nodiscard]] T readOutputValue(const TraceOutputStorage& output, std::size_t offset) noexcept
{
    static_assert(sizeof(T) <= kOutputCapacity);
    if (offset > kOutputCapacity - sizeof(T))
        return {};

    std::array<std::byte, sizeof(T)> bytes{};
    for (std::size_t i = 0; i < sizeof(T); ++i)
        bytes[i] = output.storage[offset + i];
    return std::bit_cast<T>(bytes);
}

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
    return isValidOutputOffset(endPositionOffset, sizeof(Vector))
        && isValidOutputOffset(normalOffset, sizeof(Vector))
        && isValidOutputOffset(fractionOffset, sizeof(float))
        && !doRegionsOverlap(static_cast<std::size_t>(endPositionOffset), sizeof(Vector), static_cast<std::size_t>(normalOffset), sizeof(Vector))
        && !doRegionsOverlap(static_cast<std::size_t>(endPositionOffset), sizeof(Vector), static_cast<std::size_t>(fractionOffset), sizeof(float))
        && !doRegionsOverlap(static_cast<std::size_t>(normalOffset), sizeof(Vector), static_cast<std::size_t>(fractionOffset), sizeof(float));
}

[[nodiscard]] constexpr bool isValidRawEntityHandleOffset(std::int32_t rawEntityHandleOffset, std::int32_t endPositionOffset,
    std::int32_t normalOffset, std::int32_t fractionOffset) noexcept
{
    return isValidOutputOffset(rawEntityHandleOffset, sizeof(std::int32_t))
        && !doRegionsOverlap(static_cast<std::size_t>(rawEntityHandleOffset), sizeof(std::int32_t), static_cast<std::size_t>(endPositionOffset), sizeof(Vector))
        && !doRegionsOverlap(static_cast<std::size_t>(rawEntityHandleOffset), sizeof(std::int32_t), static_cast<std::size_t>(normalOffset), sizeof(Vector))
        && !doRegionsOverlap(static_cast<std::size_t>(rawEntityHandleOffset), sizeof(std::int32_t), static_cast<std::size_t>(fractionOffset), sizeof(float));
}

}
