#pragma once

#include <CS2/Classes/Vector.h>
#include <Features/Visuals/GrenadePrediction/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionMirror.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionParams.h>
#include <Features/Visuals/GrenadePrediction/Trajectory.h>
#include <GameClient/EngineTrace/EngineTrace.h>
#include <GameClient/GrenadePrediction/GrenadeLaunch.h>
#include <Utils/Math.h>

template <typename HookContext>
class GrenadeSimulator {
public:
    explicit GrenadeSimulator(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void setPlayerCollisionSnapshot(const GrenadePlayerCollisionSnapshot* snapshot) noexcept
    {
        playerCollisionSnapshot = snapshot;
    }

    void simulate(Trajectory& trajectory, const GrenadeLaunchState& launch, cs2::GrenadeKind kind, void* skipEntity, float serverGravity) noexcept
    {
        trajectory.clear();
        trajectoryOutput = &trajectory;
        playerResponseUsed = false;

        if (!validKind(kind) || !finite(launch.origin) || !finite(launch.velocity) || !Math::isFinite(serverGravity) || serverGravity <= 0.0f) {
            trajectoryOutput = nullptr;
            return;
        }

        trajectory.endPos = launch.origin;

        auto position = launch.origin;
        auto velocity = launch.velocity;
        int pointTimer{};
        int bounceCount{};
        bool landedOnSurface{};
        for (int tick{}; tick < grenade_prediction_params::kMaxTicks; ++tick) {
            if (pointTimer == 0 && !trajectory.appendPoint(position)) {
                invalidate(trajectory, launch.origin);
                return;
            }

            const auto previousPosition = position;
            const auto result = step(position, velocity, kind, skipEntity, serverGravity);
            if (!result.traceSucceeded || !finite(position) || !finite(velocity)) {
                invalidate(trajectory, launch.origin);
                return;
            }

            bounceCount += result.contactsCount;
            landedOnSurface = landedOnSurface || result.impactDetonate;
            const bool stopped = (kind == cs2::GrenadeKind::SmokeGrenade || kind == cs2::GrenadeKind::Decoy)
                && (position - previousPosition).squareLength() < grenade_prediction_params::kStopDisplacementSq;
            if (result.impactDetonate || stopped || bounceCount > grenade_prediction_params::kMaxBounces || shouldDetonate(kind, tick)) {
                trajectory.endPos = position;
                trajectory.valid = true;
                if (kind == cs2::GrenadeKind::Molotov || kind == cs2::GrenadeKind::Incendiary)
                    trajectory.validLanding = landedOnSurface;
                break;
            }
            if (result.hit || ++pointTimer >= grenade_prediction_params::kTicksPerPoint)
                pointTimer = 0;
        }

        trajectoryOutput = nullptr;
        if (!trajectory.valid)
            return;
        if (trajectory.pointsCount == 0 || !finite(trajectory.endPos)) {
            invalidate(trajectory, launch.origin);
            return;
        }
        if (trajectory.points[trajectory.pointsCount - 1].squareDistTo(trajectory.endPos) > 1.0f && !trajectory.appendPoint(trajectory.endPos))
            invalidate(trajectory, launch.origin);
    }

private:
    struct StepResult {
        bool traceSucceeded{true};
        bool impactDetonate{};
        bool hit{};
        int contactsCount{};
    };

    [[nodiscard]] static bool finite(cs2::Vector value) noexcept
    {
        return grenade_player_collision_mirror::finite(value);
    }

    [[nodiscard]] static bool validKind(cs2::GrenadeKind kind) noexcept
    {
        return kind != cs2::GrenadeKind::None;
    }

    [[nodiscard]] static bool validTrace(const Optional<TraceResult>& trace) noexcept
    {
        return trace.hasValue() && Math::isFinite(trace.value().fraction) && trace.value().fraction >= 0.0f && trace.value().fraction <= 1.0f
            && finite(trace.value().endPos) && finite(trace.value().normal);
    }

    void invalidate(Trajectory& trajectory, cs2::Vector start) noexcept
    {
        trajectory.clear();
        trajectory.endPos = start;
        trajectoryOutput = nullptr;
    }

    [[nodiscard]] StepResult step(cs2::Vector& position, cs2::Vector& velocity, cs2::GrenadeKind kind, void* skipEntity, float serverGravity) noexcept
    {
        StepResult result;
        if (!playerResponseUsed && playerCollisionSnapshot) {
            if (const auto* candidate = grenade_player_collision_mirror::select(*playerCollisionSnapshot, position);
                candidate && grenade_player_collision_mirror::apply(position, *candidate, velocity)) {
                playerResponseUsed = true;
                if (!appendPlayerResponsePoint(position))
                    return {.traceSucceeded = false};
            }
        }

        for (int substep{}; substep < grenade_prediction_params::kMovementSubsteps; ++substep) {
            const float oldZ = velocity.z;
            velocity.z -= serverGravity * grenade_prediction_params::kGravityScale * grenade_prediction_params::kMovementSubstepDt;
            const auto movement = cs2::Vector{velocity.x * grenade_prediction_params::kMovementSubstepDt, velocity.y * grenade_prediction_params::kMovementSubstepDt,
                (oldZ + velocity.z) * 0.5f * grenade_prediction_params::kMovementSubstepDt};
            if (!finite(movement))
                return {.traceSucceeded = false};

            const auto trace = hookContext.template make<EngineTrace>().traceGrenadeHull(position, position + movement, skipEntity,
                grenade_prediction_params::kInFlightTraceMask, grenade_prediction_params::kInFlightTraceCollisionGroup,
                grenade_prediction_params::kInFlightTraceQueryByte);
            if (!validTrace(trace))
                return {.traceSucceeded = false};
            if (trace.value().fraction >= 1.0f) {
                position = position + movement;
                continue;
            }

            position = trace.value().endPos;
            result.hit = true;
            ++result.contactsCount;
            if (!appendWorldContactPoint(position))
                return {.traceSucceeded = false};

            auto bounce = clipVelocity(velocity, trace.value().normal, 2.0f) * grenade_prediction_params::kElasticity;
            const float speedSq = bounce.squareLength();
            if (!finite(bounce) || !Math::isFinite(speedSq))
                return {.traceSucceeded = false};
            if ((kind == cs2::GrenadeKind::Molotov || kind == cs2::GrenadeKind::Incendiary)
                && (trace.value().normal.z >= grenade_prediction_params::kMolotovSlope || speedSq < grenade_prediction_params::kStopSpeedSq)) {
                velocity = {};
                result.impactDetonate = true;
                return result;
            }
            velocity = speedSq < grenade_prediction_params::kStopSpeedSq ? cs2::Vector{} : bounce;
        }
        return result;
    }

    [[nodiscard]] static cs2::Vector clipVelocity(cs2::Vector velocity, cs2::Vector normal, float overbounce) noexcept
    {
        const float projected = -velocity.dot(normal) * overbounce;
        const float backoff = (projected > 0.0f ? projected : 0.0f) + grenade_prediction_params::kClipPushOff;
        return velocity + normal * backoff;
    }

    [[nodiscard]] bool appendWorldContactPoint(cs2::Vector point) noexcept
    {
        return trajectoryOutput && trajectoryOutput->appendPoint(point) && trajectoryOutput->appendWorldContactMarker();
    }

    [[nodiscard]] bool appendPlayerResponsePoint(cs2::Vector point) noexcept
    {
        return trajectoryOutput && trajectoryOutput->appendPoint(point) && trajectoryOutput->appendPlayerResponseMarker();
    }

    [[nodiscard]] static bool shouldDetonate(cs2::GrenadeKind kind, int tick) noexcept
    {
        const float elapsed = static_cast<float>(tick + 1) * grenade_prediction_params::kSimDt;
        switch (kind) {
        case cs2::GrenadeKind::Flashbang:
        case cs2::GrenadeKind::HEGrenade:
            return elapsed > grenade_prediction_params::kDetonateTimeHeFlash + grenade_prediction_params::kClientTracerHorizonPadding;
        case cs2::GrenadeKind::Molotov:
        case cs2::GrenadeKind::Incendiary:
            return elapsed > grenade_prediction_params::kDetonateTimeMolotov + grenade_prediction_params::kClientTracerHorizonPadding;
        case cs2::GrenadeKind::Decoy:
            return elapsed > grenade_prediction_params::kDetonateTimeDecoy;
        case cs2::GrenadeKind::SmokeGrenade:
            return elapsed > grenade_prediction_params::kDetonateTimeSmokeCap;
        default:
            return false;
        }
    }

    HookContext& hookContext;
    const GrenadePlayerCollisionSnapshot* playerCollisionSnapshot{};
    Trajectory* trajectoryOutput{};
    bool playerResponseUsed{};
};
