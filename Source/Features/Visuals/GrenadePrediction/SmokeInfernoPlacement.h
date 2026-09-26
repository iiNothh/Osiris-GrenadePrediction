#pragma once

#include <cstddef>
#include <cstdint>

#include <CS2/Classes/Vector.h>
#include <CS2/Constants/InteractionLayers.h>
#include <GameClient/EngineTrace/GrenadeTraceParams.h>
#include <GameClient/EngineTrace/HullTraceRequest.h>
#include <GameClient/EngineTrace/TraceResult.h>
#include <GameClient/Entities/GrenadeKind.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

struct SmokeInfernoSnapshot {
    static constexpr std::size_t kMaxFireAreas{64};

    cs2::Vector origin{};
    cs2::Vector firePositions[kMaxFireAreas]{};
    bool fireIsBurning[kMaxFireAreas]{};
    std::int32_t fireCount{};
    float fireLifetime{};
};

class SmokeInfernoScan {
public:
    static constexpr std::size_t kMaxInfernos{64};

    void beginScan() noexcept
    {
        nextCount = 0;
        overflowed = false;
        malformed = false;
        scanComplete = false;
    }

    void observe(const SmokeInfernoSnapshot& inferno) noexcept
    {
        if (!isValid(inferno))
        {
            malformed = true;
            return;
        }
        if (nextCount == kMaxInfernos) {
            overflowed = true;
            return;
        }
        nextInfernos[nextCount++] = inferno;
    }

    void markMalformed() noexcept { malformed = true; }

    [[nodiscard]] bool endScan() noexcept
    {
        scanComplete = true;
        const bool changed = overflowed != publishedOverflowed || malformed != publishedMalformed || nextCount != infernoCount
            || !sameSnapshots(nextInfernos, nextCount, infernos, infernoCount);
        if (changed) {
            for (std::size_t i{}; i < nextCount; ++i)
                infernos[i] = nextInfernos[i];
            infernoCount = nextCount;
            publishedOverflowed = overflowed;
            publishedMalformed = malformed;
            ++revision;
        }
        return changed;
    }

    [[nodiscard]] bool hasAuthoritativeScan() const noexcept { return scanComplete && !overflowed && !malformed; }
    [[nodiscard]] const SmokeInfernoSnapshot* data() const noexcept { return infernos; }
    [[nodiscard]] std::size_t size() const noexcept { return infernoCount; }
    [[nodiscard]] std::uint64_t currentRevision() const noexcept { return revision; }

private:
    [[nodiscard]] static bool isValid(const SmokeInfernoSnapshot& inferno) noexcept
    {
        if (!inferno.origin.isFinite() || inferno.fireLifetime != inferno.fireLifetime || inferno.fireLifetime <= 0.0f
            || inferno.fireCount < 0 || inferno.fireCount > static_cast<std::int32_t>(SmokeInfernoSnapshot::kMaxFireAreas))
            return false;
        for (std::int32_t i{}; i < inferno.fireCount; ++i) {
            if (!inferno.firePositions[i].isFinite())
                return false;
        }
        return true;
    }

    [[nodiscard]] static bool sameSnapshot(const SmokeInfernoSnapshot& lhs, const SmokeInfernoSnapshot& rhs) noexcept
    {
        if (lhs.origin.x != rhs.origin.x || lhs.origin.y != rhs.origin.y || lhs.origin.z != rhs.origin.z
            || lhs.fireCount != rhs.fireCount || lhs.fireLifetime != rhs.fireLifetime)
            return false;
        for (std::int32_t i{}; i < lhs.fireCount; ++i) {
            if (lhs.firePositions[i].x != rhs.firePositions[i].x || lhs.firePositions[i].y != rhs.firePositions[i].y
                || lhs.firePositions[i].z != rhs.firePositions[i].z || lhs.fireIsBurning[i] != rhs.fireIsBurning[i])
                return false;
        }
        return true;
    }

    [[nodiscard]] static bool sameSnapshots(const SmokeInfernoSnapshot* lhs, std::size_t lhsCount,
        const SmokeInfernoSnapshot* rhs, std::size_t rhsCount) noexcept
    {
        if (lhsCount != rhsCount)
            return false;
        for (std::size_t i{}; i < lhsCount; ++i) {
            if (!sameSnapshot(lhs[i], rhs[i]))
                return false;
        }
        return true;
    }

    SmokeInfernoSnapshot infernos[kMaxInfernos]{};
    SmokeInfernoSnapshot nextInfernos[kMaxInfernos]{};
    std::size_t infernoCount{};
    std::size_t nextCount{};
    std::uint64_t revision{};
    bool scanComplete{true};
    bool overflowed{};
    bool publishedOverflowed{};
    bool malformed{};
    bool publishedMalformed{};
};

class SmokeInfernoPlacement {
public:
    static constexpr cs2::Vector kPointHullMins{};
    static constexpr cs2::Vector kPointHullMaxs{};
    static constexpr cs2::Vector kFinalHullMins{-2.0f, -2.0f, -2.0f};
    static constexpr cs2::Vector kFinalHullMaxs{2.0f, 2.0f, 2.0f};

    [[nodiscard]] static bool isInsideFireArea(cs2::Vector point, cs2::Vector base) noexcept
    {
        if (!point.isFinite() || !base.isFinite())
            return false;
        const float dx = point.x - base.x;
        const float dy = point.y - base.y;
        const float horizontalDistanceSquared = dx * dx + dy * dy;
        if (!Math::isFinite(horizontalDistanceSquared) || horizontalDistanceSquared >= kRadiusSquared)
            return false;
        const float verticalDistanceFromCenter = Math::abs(point.z - (base.z + kFireAreaHalfHeight)) - kFireAreaHalfHeight;
        if (!Math::isFinite(verticalDistanceFromCenter))
            return false;
        return verticalDistanceFromCenter <= 0.0f || horizontalDistanceSquared + verticalDistanceFromCenter * verticalDistanceFromCenter < kRadiusSquared;
    }

    template <typename Trace>
    [[nodiscard]] static Optional<cs2::Vector> correctedEndpoint(GrenadeKind kind, cs2::Vector contactPosition,
        const SmokeInfernoScan& infernos, Trace&& trace) noexcept
    {
        if (kind != GrenadeKind::SmokeGrenade || !contactPosition.isFinite() || !infernos.hasAuthoritativeScan())
            return {};
        const auto target = selectTarget(contactPosition, infernos, trace);
        if (!target.hasValue())
            return {};

        const auto finalTrace = trace(makeFinalRequest(contactPosition, target.value()));
        if (!hasFiniteEndpoint(finalTrace))
            return {};
        return finalTrace.value().endPos;
    }

    template <typename Trace>
    [[nodiscard]] static Optional<cs2::Vector> selectTarget(cs2::Vector terminalPosition, const SmokeInfernoScan& infernos, Trace&& trace) noexcept
    {
        if (!terminalPosition.isFinite() || !infernos.hasAuthoritativeScan())
            return {};

        Optional<cs2::Vector> selectedTarget;
        float bestDistanceSquared{};
        for (std::size_t infernoIndex{}; infernoIndex < infernos.size(); ++infernoIndex) {
            const auto& inferno = infernos.data()[infernoIndex];
            const auto area = firstEligibleArea(terminalPosition, inferno, trace);
            if (!area.hasValue())
                continue;

            const auto distanceSquared = (terminalPosition - inferno.origin).squareLength();
            if (!Math::isFinite(distanceSquared))
                continue;
            if (!selectedTarget.hasValue() || distanceSquared < bestDistanceSquared) {
                selectedTarget = area;
                bestDistanceSquared = distanceSquared;
            }
        }
        return selectedTarget;
    }

    [[nodiscard]] static engine_trace::TraceFilterParameters makeLosFilter(bool raisedRetry) noexcept
    {
        using InteractionLayer = cs2::engine_trace::InteractionLayer;
        return {
            .interactsWith = InteractionLayer::Solid | InteractionLayer::Window,
            .collisionGroup = engine_trace::grenade::kCollisionGroup,
            .queryFlags = engine_trace::grenade::kQueryFlags,
            .interactsExclude = raisedRetry ? InteractionLayer::Player : InteractionLayer::Player | InteractionLayer::Debris,
            .interactsAs = engine_trace::grenade::kFilterObjectMask
        };
    }

    [[nodiscard]] static engine_trace::TraceFilterParameters makeFinalFilter() noexcept
    {
        using InteractionLayer = cs2::engine_trace::InteractionLayer;
        return {
            .interactsWith = InteractionLayer::Solid | InteractionLayer::Window | InteractionLayer::PassBullets | InteractionLayer::CsgoGrenadeClip,
            .collisionGroup = engine_trace::grenade::kCollisionGroup,
            .queryFlags = engine_trace::grenade::kQueryFlags
        };
    }

private:
    static constexpr float kRadiusSquared{4900.0f};
    static constexpr float kFireAreaHeight{40.0f};
    static constexpr float kFireAreaHalfHeight{kFireAreaHeight * 0.5f};
    static constexpr float kLosRaisedOffset{30.0f};

    [[nodiscard]] static engine_trace::HullTraceRequest makeLosRequest(cs2::Vector start, cs2::Vector end, bool raisedRetry) noexcept
    {
        return {
            .start = start,
            .end = end,
            .mins = kPointHullMins,
            .maxs = kPointHullMaxs,
            .filter = makeLosFilter(raisedRetry)
        };
    }

    [[nodiscard]] static engine_trace::HullTraceRequest makeFinalRequest(cs2::Vector start, cs2::Vector end) noexcept
    {
        return {
            .start = start,
            .end = end,
            .mins = kFinalHullMins,
            .maxs = kFinalHullMaxs,
            .filter = makeFinalFilter()
        };
    }

    [[nodiscard]] static bool hasFiniteFraction(const Optional<TraceResult>& trace) noexcept
    {
        return trace.hasValue() && Math::isFinite(trace.value().fraction);
    }

    [[nodiscard]] static bool hasFiniteEndpoint(const Optional<TraceResult>& trace) noexcept
    {
        return trace.hasValue() && trace.value().endPos.isFinite();
    }

    template <typename Trace>
    [[nodiscard]] static Optional<cs2::Vector> firstEligibleArea(cs2::Vector terminalPosition, const SmokeInfernoSnapshot& inferno, Trace&& trace) noexcept
    {
        for (std::int32_t i{}; i < inferno.fireCount; ++i) {
            const auto base = inferno.firePositions[i];
            if (!inferno.fireIsBurning[i] || !isInsideFireArea(terminalPosition, base) || !hasLineOfSight(terminalPosition, base, trace))
                continue;
            return base;
        }
        return {};
    }

    template <typename Trace>
    [[nodiscard]] static bool hasLineOfSight(cs2::Vector terminalPosition, cs2::Vector base, Trace&& trace) noexcept
    {
        const auto top = base + cs2::Vector{0.0f, 0.0f, kFireAreaHeight};
        const auto initialResult = lineOfSightRound(terminalPosition, top, false, trace);
        if (initialResult == LineOfSightResult::Open)
            return true;
        if (initialResult == LineOfSightResult::Invalid)
            return false;

        const auto raisedTrace = trace(makeLosRequest(terminalPosition, terminalPosition + cs2::Vector{0.0f, 0.0f, kLosRaisedOffset}, true));
        if (!hasFiniteEndpoint(raisedTrace))
            return false;
        return lineOfSightRound(raisedTrace.value().endPos, top, true, trace) == LineOfSightResult::Open;
    }

    enum class LineOfSightResult { Open, Blocked, Invalid };

    template <typename Trace>
    [[nodiscard]] static LineOfSightResult lineOfSightRound(cs2::Vector terminalPosition, cs2::Vector top, bool raisedRetry, Trace&& trace) noexcept
    {
        const auto raisedStart = top + cs2::Vector{0.0f, 0.0f, kLosRaisedOffset};
        const auto firstTrace = trace(makeLosRequest(raisedStart, terminalPosition, raisedRetry));
        if (!hasFiniteFraction(firstTrace))
            return LineOfSightResult::Invalid;
        if (firstTrace.value().fraction >= 1.0f)
            return LineOfSightResult::Open;
        const auto secondTrace = trace(makeLosRequest(top, terminalPosition, raisedRetry));
        if (!hasFiniteFraction(secondTrace))
            return LineOfSightResult::Invalid;
        return secondTrace.value().fraction >= 1.0f ? LineOfSightResult::Open : LineOfSightResult::Blocked;
    }
};
