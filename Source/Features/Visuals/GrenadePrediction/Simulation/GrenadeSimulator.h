#pragma once

#include <cstddef>

#include <CS2/Classes/Vector.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeSimulationParameters.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeSimulationResult.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

namespace grenade_prediction
{

struct GrenadeTraceResult {
    float fraction{};
    cs2::Vector endPosition{};
    cs2::Vector normal{};
};

struct GrenadeSimulationInput {
    cs2::Vector startPosition{};
    cs2::Vector initialVelocity{};
    GrenadeKind kind{GrenadeKind::None};
    void* ignoredEntity{};
};

template <typename Trace>
class GrenadeSimulator {
public:
    explicit GrenadeSimulator(Trace& trace) noexcept
        : trace{trace}
    {
    }

    void simulate(const GrenadeSimulationParameters& parameters, const GrenadeSimulationInput& input, GrenadeSimulationResult& result) noexcept
    {
        result.trajectory.reset(input.startPosition);
        result.traceFailed = false;
        result.detonated = false;
        if (!isFinite(input.startPosition) || !isFinite(input.initialVelocity) || parameters.movementSubsteps == 0 || parameters.tickInterval <= 0.0f)
            return;

        cs2::Vector position = input.startPosition;
        cs2::Vector velocity = input.initialVelocity;
        std::size_t bounceCount = 0;
        for (std::size_t tick = 0; tick < parameters.maximumTicks; ++tick) {
            if (parameters.pointIntervalTicks == 0 || tick % parameters.pointIntervalTicks == 0)
                result.trajectory.appendPoint(position);

            const cs2::Vector previousPosition = position;
            for (std::size_t substep = 0; substep < parameters.movementSubsteps; ++substep) {
                const auto movement = move(parameters, input, position, velocity, result.trajectory, bounceCount);
                if (movement == MovementResult::TraceFailed) {
                    result.trajectory.reset(input.startPosition);
                    result.traceFailed = true;
                    return;
                }
                if (movement == MovementResult::ImpactDetonated) {
                    finish(result, position, true);
                    return;
                }
                if (bounceCount > parameters.maximumBounces) {
                    finish(result, position, false);
                    return;
                }
            }

            if ((input.kind == GrenadeKind::SmokeGrenade || input.kind == GrenadeKind::Decoy)
                && (position - previousPosition).squareLength() < parameters.stoppedDisplacementSquared) {
                finish(result, position, hasLandingMarker(input.kind));
                return;
            }
            if (shouldDetonate(parameters, input.kind, tick + 1)) {
                finish(result, position, hasLandingMarker(input.kind));
                return;
            }
        }
    }

    [[nodiscard]] static cs2::Vector clipVelocity(cs2::Vector velocity, cs2::Vector normal, float overbounce, float pushOff) noexcept
    {
        const float backoff = Math::maximum(-velocity.dot(normal) * overbounce, 0.0f) + pushOff;
        return velocity + normal * backoff;
    }

    [[nodiscard]] static bool shouldDetonate(const GrenadeSimulationParameters& parameters, GrenadeKind kind, std::size_t elapsedTicks) noexcept
    {
        const float elapsedTime = static_cast<float>(elapsedTicks) * parameters.tickInterval;
        switch (kind) {
        case GrenadeKind::Flashbang:
        case GrenadeKind::HEGrenade: return elapsedTime > parameters.heAndFlashDetonationTime;
        case GrenadeKind::Molotov:
        case GrenadeKind::Incendiary: return elapsedTime > parameters.molotovDetonationTime;
        case GrenadeKind::Decoy: return elapsedTime > parameters.decoyDetonationTime;
        case GrenadeKind::SmokeGrenade: return elapsedTime > parameters.smokeDetonationTime;
        default: return false;
        }
    }

private:
    enum class MovementResult { Continued, ImpactDetonated, TraceFailed };

    [[nodiscard]] MovementResult move(const GrenadeSimulationParameters& parameters, const GrenadeSimulationInput& input,
        cs2::Vector& position, cs2::Vector& velocity, GrenadeTrajectory& trajectory, std::size_t& bounceCount) noexcept
    {
        const float substepInterval = parameters.tickInterval / static_cast<float>(parameters.movementSubsteps);
        const float oldVelocityZ = velocity.z;
        velocity.z -= parameters.gravity * parameters.gravityScale * substepInterval;
        const cs2::Vector displacement{velocity.x * substepInterval, velocity.y * substepInterval, (oldVelocityZ + velocity.z) * 0.5f * substepInterval};
        const auto primaryTrace = trace.traceGrenadeHull(position, position + displacement, input.ignoredEntity);
        if (!primaryTrace.hasValue())
            return MovementResult::TraceFailed;
        if (primaryTrace.value().fraction >= 1.0f) {
            position = position + displacement;
            return MovementResult::Continued;
        }

        const auto& contact = primaryTrace.value();
        position = contact.endPosition;
        trajectory.appendBounce(position);
        ++bounceCount;
        const cs2::Vector bounceVelocity = clipVelocity(velocity, contact.normal, 2.0f, parameters.contactPushOff) * parameters.elasticity;
        if ((input.kind == GrenadeKind::Molotov || input.kind == GrenadeKind::Incendiary)
            && (contact.normal.z >= parameters.molotovSlopeNormalZ || bounceVelocity.squareLength() < parameters.stopSpeedSquared)) {
            return MovementResult::ImpactDetonated;
        }
        if (bounceVelocity.squareLength() < parameters.stopSpeedSquared) {
            velocity = {};
            return MovementResult::Continued;
        }

        velocity = bounceVelocity;
        const auto continuationTrace = trace.traceGrenadeHull(position, position + velocity * ((1.0f - contact.fraction) * substepInterval), input.ignoredEntity);
        if (!continuationTrace.hasValue())
            return MovementResult::TraceFailed;
        position = continuationTrace.value().fraction >= 1.0f ? position + velocity * ((1.0f - contact.fraction) * substepInterval) : continuationTrace.value().endPosition;
        return MovementResult::Continued;
    }

    static void finish(GrenadeSimulationResult& result, cs2::Vector position, bool validLanding) noexcept
    {
        result.trajectory.endPosition = position;
        result.trajectory.valid = true;
        result.trajectory.validLanding = validLanding;
        result.detonated = true;
        if (result.trajectory.pointCount == 0 || result.trajectory.points[result.trajectory.pointCount - 1].squareDistTo(position) > 1.0f)
            result.trajectory.appendPoint(position);
    }

    [[nodiscard]] static bool hasLandingMarker(GrenadeKind kind) noexcept
    {
        return kind != GrenadeKind::Molotov && kind != GrenadeKind::Incendiary;
    }

    [[nodiscard]] static bool isFinite(cs2::Vector value) noexcept
    {
        return Math::isFinite(value.x) && Math::isFinite(value.y) && Math::isFinite(value.z);
    }

    Trace& trace;
};

}
