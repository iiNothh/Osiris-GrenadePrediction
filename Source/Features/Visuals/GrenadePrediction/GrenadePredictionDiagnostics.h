#pragma once

#include <cstddef>
#include <cstdint>

enum class GrenadePredictionDiagnosticKind : std::uint8_t {
    LiveCacheOverflow,
    CollisionOverflow,
    RelevantPlayerInvalid,
    MalformedUnrelatedIdentitySkipped,
    LiveSimulationFailure,
    LiveSimulationBackoff,
    Count
};

class GrenadePredictionDiagnostics {
public:
    static constexpr std::uint32_t reportCooldownFrames{120};
    static constexpr std::size_t kindCount{static_cast<std::size_t>(GrenadePredictionDiagnosticKind::Count)};

    static_assert(kindCount <= sizeof(std::uint8_t) * 8);

    void record(GrenadePredictionDiagnosticKind kind, std::uint32_t frame) noexcept
    {
        const auto index = toIndex(kind);
        if (counters[index] != ~std::uint32_t{})
            ++counters[index];

        if (!hasReported[index] || frame - lastReportedFrames[index] >= reportCooldownFrames) {
            reportableKinds |= kindBit(kind);
            lastReportedFrames[index] = frame;
            hasReported[index] = true;
        }
    }

    [[nodiscard]] std::uint32_t count(GrenadePredictionDiagnosticKind kind) const noexcept
    {
        return counters[toIndex(kind)];
    }

    [[nodiscard]] bool isReportable(GrenadePredictionDiagnosticKind kind) const noexcept
    {
        return (reportableKinds & kindBit(kind)) != 0;
    }

    [[nodiscard]] std::uint8_t reportableKindBits() const noexcept
    {
        return reportableKinds;
    }

    void clearReportable(GrenadePredictionDiagnosticKind kind) noexcept
    {
        reportableKinds &= static_cast<std::uint8_t>(~kindBit(kind));
    }

    void clearReportable() noexcept
    {
        reportableKinds = 0;
    }

private:
    [[nodiscard]] static constexpr std::size_t toIndex(GrenadePredictionDiagnosticKind kind) noexcept
    {
        return static_cast<std::size_t>(kind);
    }

    [[nodiscard]] static constexpr std::uint8_t kindBit(GrenadePredictionDiagnosticKind kind) noexcept
    {
        return static_cast<std::uint8_t>(1U << toIndex(kind));
    }

    std::uint32_t counters[kindCount]{};
    std::uint32_t lastReportedFrames[kindCount]{};
    bool hasReported[kindCount]{};
    std::uint8_t reportableKinds{};
};
