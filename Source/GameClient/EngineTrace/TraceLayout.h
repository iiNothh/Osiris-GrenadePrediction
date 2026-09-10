#pragma once

#include <cstddef>
#include <cstdint>

#include <CS2/Classes/Vector.h>
#include <CS2/Constants/InteractionLayers.h>
#include <CS2/EngineTrace/CGameTrace.h>
#include <CS2/EngineTrace/CTraceFilter.h>
#include <Utils/Optional.h>

namespace engine_trace {
    [[nodiscard]] constexpr bool regionsOverlap(std::size_t firstOffset, std::size_t firstLength, std::size_t secondOffset, std::size_t secondLength) noexcept
    {
        if (firstOffset <= secondOffset)
            return firstLength > secondOffset - firstOffset;
        return secondLength > firstOffset - secondOffset;
    }

    [[nodiscard]] constexpr bool isValidCGameTraceOutputOffset(std::int32_t offset, std::size_t fieldSize) noexcept
    {
        return offset > 0
            && static_cast<std::size_t>(offset) % alignof(float) == 0
            && static_cast<std::size_t>(offset) <= cs2::engine_trace::kCGameTraceCapacity
            && fieldSize <= cs2::engine_trace::kCGameTraceCapacity - static_cast<std::size_t>(offset);
    }

    [[nodiscard]] constexpr bool areValidCGameTraceOutputOffsets(std::int32_t endPositionOffset, std::int32_t normalOffset, std::int32_t fractionOffset) noexcept
    {
        return isValidCGameTraceOutputOffset(endPositionOffset, sizeof(cs2::Vector))
            && isValidCGameTraceOutputOffset(normalOffset, sizeof(cs2::Vector))
            && isValidCGameTraceOutputOffset(fractionOffset, sizeof(float))
            && !regionsOverlap(static_cast<std::size_t>(endPositionOffset), sizeof(cs2::Vector), static_cast<std::size_t>(normalOffset), sizeof(cs2::Vector))
            && !regionsOverlap(static_cast<std::size_t>(endPositionOffset), sizeof(cs2::Vector), static_cast<std::size_t>(fractionOffset), sizeof(float))
            && !regionsOverlap(static_cast<std::size_t>(normalOffset), sizeof(cs2::Vector), static_cast<std::size_t>(fractionOffset), sizeof(float));
    }

    [[nodiscard]] constexpr bool isValidCGameTraceRawEntityHandleOffset(std::int32_t rawEntityHandleOffset, std::int32_t endPositionOffset, std::int32_t normalOffset, std::int32_t fractionOffset) noexcept
    {
        return isValidCGameTraceOutputOffset(rawEntityHandleOffset, sizeof(std::int32_t))
            && !regionsOverlap(static_cast<std::size_t>(rawEntityHandleOffset), sizeof(std::int32_t), static_cast<std::size_t>(endPositionOffset), sizeof(cs2::Vector))
            && !regionsOverlap(static_cast<std::size_t>(rawEntityHandleOffset), sizeof(std::int32_t), static_cast<std::size_t>(normalOffset), sizeof(cs2::Vector))
            && !regionsOverlap(static_cast<std::size_t>(rawEntityHandleOffset), sizeof(std::int32_t), static_cast<std::size_t>(fractionOffset), sizeof(float));
    }

    struct TraceOutputLayout {
        std::int32_t endPositionOffset{};
        std::int32_t normalOffset{};
        std::int32_t fractionOffset{};
        Optional<std::int32_t> rawEntityHandleOffset{};

        [[nodiscard]] bool hasValidRawEntityHandleOffset() const noexcept
        {
            return rawEntityHandleOffset.hasValue()
                && isValidCGameTraceRawEntityHandleOffset(rawEntityHandleOffset.value(), endPositionOffset, normalOffset, fractionOffset);
        }
    };

    [[nodiscard]] constexpr bool hasValidTraceOutputLayout(const TraceOutputLayout& layout) noexcept
    {
        return areValidCGameTraceOutputOffsets(layout.endPositionOffset, layout.normalOffset, layout.fractionOffset);
    }
}

namespace engine_trace::grenade {
    struct FilterOverlayLayout {
        std::uint8_t interactsExcludeOffset{};
        std::uint8_t interactsAsOffset{};
        std::uint8_t flagsOffset{};
        std::uint8_t candidateCollectionModeOffset{};
    };

    [[nodiscard]] constexpr bool isValidFilterFieldOffset(std::size_t offset, std::size_t size, std::size_t alignment = 1) noexcept
    {
        return offset >= sizeof(void*) && offset % alignment == 0
            && offset <= cs2::engine_trace::kCTraceFilterCapacity && size <= cs2::engine_trace::kCTraceFilterCapacity - offset;
    }

    [[nodiscard]] constexpr bool hasValidFilterOverlayLayout(const FilterOverlayLayout& layout) noexcept
    {
        constexpr auto kInteractionLayerSize = sizeof(cs2::engine_trace::InteractionLayer);
        constexpr auto kByteSize = sizeof(std::uint8_t);
        return isValidFilterFieldOffset(layout.interactsExcludeOffset, kInteractionLayerSize, alignof(cs2::engine_trace::InteractionLayer))
            && isValidFilterFieldOffset(layout.interactsAsOffset, kInteractionLayerSize, alignof(cs2::engine_trace::InteractionLayer))
            && isValidFilterFieldOffset(layout.flagsOffset, kByteSize)
            && isValidFilterFieldOffset(layout.candidateCollectionModeOffset, kByteSize)
            && !regionsOverlap(layout.interactsExcludeOffset, kInteractionLayerSize, layout.interactsAsOffset, kInteractionLayerSize)
            && !regionsOverlap(layout.interactsExcludeOffset, kInteractionLayerSize, layout.flagsOffset, kByteSize)
            && !regionsOverlap(layout.interactsExcludeOffset, kInteractionLayerSize, layout.candidateCollectionModeOffset, kByteSize)
            && !regionsOverlap(layout.interactsAsOffset, kInteractionLayerSize, layout.flagsOffset, kByteSize)
            && !regionsOverlap(layout.interactsAsOffset, kInteractionLayerSize, layout.candidateCollectionModeOffset, kByteSize)
            && !regionsOverlap(layout.flagsOffset, kByteSize, layout.candidateCollectionModeOffset, kByteSize);
    }

    [[nodiscard]] constexpr bool isFilterOverlayWriteOffset(const FilterOverlayLayout& layout, std::size_t offset) noexcept
    {
        return hasValidFilterOverlayLayout(layout)
            && (offset >= layout.interactsExcludeOffset && offset < layout.interactsExcludeOffset + sizeof(cs2::engine_trace::InteractionLayer)
                || offset >= layout.interactsAsOffset && offset < layout.interactsAsOffset + sizeof(cs2::engine_trace::InteractionLayer)
                || offset == layout.flagsOffset || offset == layout.candidateCollectionModeOffset);
    }
}
