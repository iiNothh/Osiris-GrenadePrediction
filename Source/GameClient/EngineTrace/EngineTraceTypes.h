#pragma once

#include <bit>
#include <cstdint>

#include <CS2/Classes/Vector.h>
#include <CS2/EngineTrace/EngineTraceTypes.h>
#include <Utils/Optional.h>

struct TraceResult {
    float fraction{};
    cs2::Vector endPos{};
    cs2::Vector normal{};
    Optional<std::int32_t> rawEntityHandle{};
};

namespace engine_trace {

struct TraceFilterExcludedEntities {
    constexpr TraceFilterExcludedEntities(void* first = nullptr, void* second = nullptr) noexcept
    {
        if (first == second)
            second = nullptr;
        this->first = first;
        this->second = second;
    }

    void* first{};
    void* second{};
};

struct TraceFilterParameters {
    std::uint64_t mask{};
    std::uint8_t collisionGroup{};
    std::uint8_t queryByte{};
};

struct HullTraceRequest {
    cs2::Vector start{};
    cs2::Vector end{};
    cs2::Vector mins{};
    cs2::Vector maxs{};
    TraceFilterExcludedEntities excludedEntities{};
    TraceFilterParameters filter{};
};

struct TraceOutputLayout {
    std::int32_t endPositionOffset{};
    std::int32_t normalOffset{};
    std::int32_t fractionOffset{};
    Optional<std::int32_t> rawEntityHandleOffset{};
};

using cs2::engine_trace::kWorldEntityHandle;

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

[[nodiscard]] constexpr bool hasValidHullBounds(cs2::Vector mins, cs2::Vector maxs) noexcept
{
    return isFinite(mins) && isFinite(maxs)
        && mins.x <= maxs.x && mins.y <= maxs.y && mins.z <= maxs.z;
}

[[nodiscard]] constexpr bool isValidHullTraceRequest(const HullTraceRequest& request) noexcept
{
    return isFinite(request.start) && isFinite(request.end)
        && hasValidHullBounds(request.mins, request.maxs);
}

[[nodiscard]] constexpr cs2::engine_trace::HullTraceDescriptor makeHullTraceDescriptor(const HullTraceRequest& request) noexcept
{
    return {.mins = request.mins, .maxs = request.maxs};
}

[[nodiscard]] constexpr bool hasValidTraceOutputLayout(const TraceOutputLayout& layout) noexcept
{
    return cs2::engine_trace::areValidOutputOffsets(layout.endPositionOffset, layout.normalOffset, layout.fractionOffset);
}

[[nodiscard]] inline Optional<TraceResult> decodeTraceOutput(const cs2::engine_trace::TraceOutputStorage& output,
    const TraceOutputLayout& layout) noexcept
{
    if (!hasValidTraceOutputLayout(layout))
        return {};

    const auto fraction = cs2::engine_trace::readOutputValue<float>(output, layout.fractionOffset);
    const auto endPosition = cs2::engine_trace::readOutputValue<cs2::Vector>(output, layout.endPositionOffset);
    const auto normal = cs2::engine_trace::readOutputValue<cs2::Vector>(output, layout.normalOffset);
    if (!isFinite(fraction) || fraction < 0.0f || fraction > 1.0f
        || !isFinite(endPosition) || !isFinite(normal)
        || (fraction < 1.0f && !hasUsableNormal(normal)))
        return {};

    if (fraction < 1.0f && layout.rawEntityHandleOffset.hasValue()
        && cs2::engine_trace::isValidRawEntityHandleOffset(layout.rawEntityHandleOffset.value(),
            layout.endPositionOffset, layout.normalOffset, layout.fractionOffset))
        return TraceResult{fraction, endPosition, normal,
            cs2::engine_trace::readOutputValue<std::int32_t>(output, layout.rawEntityHandleOffset.value())};
    return TraceResult{fraction, endPosition, normal};
}

}
