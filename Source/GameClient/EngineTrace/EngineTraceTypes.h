#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <CS2/Classes/Vector.h>
#include <Utils/MemorySection.h>

namespace engine_trace {

static_assert(std::endian::native == std::endian::little);

constexpr std::size_t kDescriptorCapacity{0x30};
constexpr std::size_t kFilterCapacity{0x48};
constexpr std::size_t kOutputCapacity{0xC0};
constexpr std::int32_t kWorldEntityHandle{0x8000};

constexpr std::uint64_t kNativePipFirstInteraction{0x0000000200003001ULL};
constexpr std::uint64_t kNativePipFilterInteractionMask{0x0000000000040200ULL};
constexpr std::uint64_t kNativePipFilterObjectMask{0x0000008000020001ULL};
constexpr std::uint8_t kNativePipCollisionGroup{0x10};
constexpr std::uint8_t kNativePipQueryByte{0x0F};

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
static_assert(sizeof(FilterStorage) == 0x48);
static_assert(alignof(FilterStorage) == alignof(void*));
static_assert(sizeof(OutputStorage) == kOutputCapacity && alignof(OutputStorage) == 16);

[[nodiscard]] constexpr bool isNativePipFilterWriteWhitelistedOffset(std::size_t offset) noexcept
{
    return (offset >= 0x08 && offset <= 0x1F) || (offset >= 0x34 && offset <= 0x39) || offset == 0x40;
}

static_assert(!isNativePipFilterWriteWhitelistedOffset(0x00));
static_assert(isNativePipFilterWriteWhitelistedOffset(0x08));
static_assert(isNativePipFilterWriteWhitelistedOffset(0x1F));
static_assert(!isNativePipFilterWriteWhitelistedOffset(0x20));
static_assert(!isNativePipFilterWriteWhitelistedOffset(0x33));
static_assert(isNativePipFilterWriteWhitelistedOffset(0x34));
static_assert(isNativePipFilterWriteWhitelistedOffset(0x39));
static_assert(!isNativePipFilterWriteWhitelistedOffset(0x3A));
static_assert(!isNativePipFilterWriteWhitelistedOffset(0x3F));
static_assert(isNativePipFilterWriteWhitelistedOffset(0x40));
static_assert(!isNativePipFilterWriteWhitelistedOffset(0x41));

template <typename T>
[[nodiscard]] T readFilterValue(const FilterStorage& filter, std::size_t offset) noexcept
{
    std::array<std::byte, sizeof(T)> bytes{};
    for (std::size_t i = 0; i < sizeof(T); ++i)
        bytes[i] = filter.storage[offset + i];
    return std::bit_cast<T>(bytes);
}

template <typename T>
void writeFilterValue(FilterStorage& filter, std::size_t offset, T value) noexcept
{
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    for (std::size_t i = 0; i < sizeof(T); ++i)
        filter.storage[offset + i] = bytes[i];
}

[[nodiscard]] inline const std::byte* nativePipBaseFilterCallback(const MemorySection& clientVmtSection, void** baseVtable) noexcept
{
    if (baseVtable == nullptr || !clientVmtSection.contains(reinterpret_cast<std::uintptr_t>(baseVtable), sizeof(void*) * 2))
        return nullptr;

    if (baseVtable[1] == nullptr)
        return nullptr;

    return static_cast<const std::byte*>(baseVtable[1]);
}

[[nodiscard]] inline bool hasExpectedNativePipBaseFilterCallback(const MemorySection& clientCodeSection, const std::byte* callback) noexcept
{
    if (callback == nullptr || !clientCodeSection.contains(reinterpret_cast<std::uintptr_t>(callback), 3))
        return false;

    return callback[0] == std::byte{0xB0} && callback[1] == std::byte{0x01} && callback[2] == std::byte{0xC3};
}

[[nodiscard]] constexpr bool hasNativePipProfileMarkers(void** baseVtable, const std::byte* pipFilterBody,
    const std::byte* pushFilterConstructor, const std::byte* primaryTraceShapeCandidate,
    const std::byte* secondaryTraceShapeCandidate, const std::byte* primaryToSecondaryCallsite,
    const std::byte* secondaryCandidateEnumerationFunction) noexcept
{
    return baseVtable != nullptr && pipFilterBody != nullptr && pushFilterConstructor != nullptr
        && primaryTraceShapeCandidate != nullptr && secondaryTraceShapeCandidate != nullptr
        && primaryToSecondaryCallsite != nullptr && secondaryCandidateEnumerationFunction != nullptr;
}

[[nodiscard]] constexpr bool hasCurrentNativePipProfileStructure(std::uintptr_t primaryTraceShapeCandidate,
    std::uintptr_t secondaryTraceShapeCandidate, std::uintptr_t primaryToSecondaryCallsite,
    std::uintptr_t secondaryCandidateEnumerationFunction) noexcept
{
    return primaryToSecondaryCallsite >= primaryTraceShapeCandidate
        && secondaryTraceShapeCandidate >= secondaryCandidateEnumerationFunction
        && primaryToSecondaryCallsite - primaryTraceShapeCandidate == 0x210
        && secondaryTraceShapeCandidate - secondaryCandidateEnumerationFunction == 0x5C3;
}

[[nodiscard]] inline bool hasExpectedNativePipFilterInitialization(const FilterStorage& filter, void** baseVtable) noexcept
{
    return readFilterValue<void*>(filter, 0x00) == baseVtable
        && filter.storage[0x40] == std::byte{}
        && (std::to_integer<unsigned char>(filter.storage[0x39]) & 0x80) == 0;
}

inline void applyNativePipFilterOverlay(FilterStorage& filter) noexcept
{
    writeFilterValue(filter, 0x08, kNativePipFirstInteraction);
    writeFilterValue(filter, 0x10, kNativePipFilterInteractionMask);
    writeFilterValue(filter, 0x18, kNativePipFilterObjectMask);
    writeFilterValue(filter, 0x34, std::uint16_t{0xFFFF});
    filter.storage[0x36] = std::byte{};
    filter.storage[0x37] = std::byte{kNativePipQueryByte};
    filter.storage[0x38] = std::byte{kNativePipCollisionGroup};
    filter.storage[0x39] = std::byte{0x4B};
    filter.storage[0x40] = std::byte{0x01};
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
