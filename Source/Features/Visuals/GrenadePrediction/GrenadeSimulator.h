#pragma once

#include <CS2/Classes/Vector.h>
#include <GameClient/Entities/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionMirror.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionParams.h>
#include <Features/Visuals/GrenadePrediction/Trajectory.h>
#include <GameClient/EngineTrace/EngineTrace.h>
#include <GameClient/EngineTrace/EngineTraceTypes.h>
#include <GameClient/Entities/EntityClassifier.h>
#include <GameClient/EntitySystem/EntitySystem.h>
#include <GameClient/GrenadePrediction/GrenadeLaunch.h>
#include <Utils/Math.h>

struct StepResult {
    bool traceSucceeded{true};
    bool impactDetonate{};
    bool hit{};
    int contactsCount{};
};

struct GrenadeContinuationInput {
    GrenadeLaunchState launch;
    float elapsedTime;
    int consumedWorldContacts;
    bool playerResponseConsumed;
    bool landedOnSurface{};
};

template <typename HookContext> struct GrenadeSimulatorTestAccess;

template <typename HookContext>
class GrenadeSimulator {
public:
    explicit GrenadeSimulator(HookContext& hookContext) noexcept : hookContext{hookContext} {}

    void setPlayerCollisionSnapshot(const GrenadePlayerCollisionSnapshot* snapshot) noexcept { configuredPlayerCollisionSnapshot = snapshot; }

    [[nodiscard]] static cs2::Vector computeInitialVelocity(cs2::Vector viewAngles, float baseVelocity, float throwStrength) noexcept
    {
        const float strength = normalizeThrowStrength(throwStrength);
        const float pitch = viewAngles.x - (90.0f - Math::abs(viewAngles.x)) * 10.0f / 90.0f;
        const float nativeVelocity = baseVelocity * 0.9f;
        const float clampedVelocity = nativeVelocity < 15.0f ? 15.0f : nativeVelocity > 750.0f ? 750.0f : nativeVelocity;
        return forwardFromAngles(pitch, viewAngles.y) * ((strength * 0.7f + 0.3f) * clampedVelocity);
    }

    [[nodiscard]] Optional<cs2::Vector> computeSpawnPosition(cs2::Vector eyePos, cs2::Vector viewAngles, float throwStrength, void* skipEntity) noexcept
    {
        if (!finite(eyePos) || !finite(viewAngles))
            return {};
        const float strength = normalizeThrowStrength(throwStrength);
        const auto forward = forwardFromAngles(viewAngles.x - (90.0f - Math::abs(viewAngles.x)) * 10.0f / 90.0f, viewAngles.y);
        if (!finite(forward))
            return {};
        eyePos.z += strength * grenade_prediction_params::kThrowZOffsetScale - grenade_prediction_params::kThrowZOffsetScale;
        const auto traceEnd = eyePos + forward * grenade_prediction_params::kSpawnTraceForward;
        const auto trace = traceGrenadeHull(eyePos, traceEnd, skipEntity);
        if (!validTrace(trace))
            return {};
        const auto hitPos = trace.value().fraction < 1.0f ? trace.value().endPos : traceEnd;
        auto spawnPos = hitPos - forward * grenade_prediction_params::kSpawnPullBack;
        if ((spawnPos - eyePos).dot(forward) < 0.0f)
            spawnPos = eyePos;
        return finite(spawnPos) ? Optional<cs2::Vector>{spawnPos} : Optional<cs2::Vector>{};
    }

    void simulate(Trajectory& trajectory, const GrenadeLaunchState& launch, GrenadeKind kind, void* skipEntity, float serverGravity) noexcept
    {
        simulateContinuation(trajectory, {launch, 0.0f, 0, false}, kind, skipEntity, serverGravity);
    }

    void simulateContinuation(Trajectory& trajectory, const GrenadeContinuationInput& input, GrenadeKind kind, void* skipEntity,
        float serverGravity) noexcept
    {
        trajectory.clear();
        trajectory.endPos = input.launch.origin;

        if (kind == GrenadeKind::None || !finite(input.launch.origin) || !finite(input.launch.velocity) || !Math::isFinite(input.elapsedTime)
            || input.elapsedTime < 0.0f || input.elapsedTime >= simulationHorizon() || input.consumedWorldContacts < 0
            || input.consumedWorldContacts > grenade_prediction_params::kMaxBounces || !Math::isFinite(serverGravity) || serverGravity <= 0.0f)
            return;

        SimulationScratch scratch{&trajectory, configuredPlayerCollisionSnapshot, input.playerResponseConsumed};
        auto position = input.launch.origin;
        auto velocity = input.launch.velocity;
        int pointTimer{};
        int bounceCount{input.consumedWorldContacts};
        bool landedOnSurface{input.landedOnSurface};
        float terminalElapsedTime{};
        if (!appendPointAtTime(trajectory, position, input.elapsedTime)) {
            invalidate(trajectory, input.launch.origin);
            return;
        }

        for (int tick{}; tick < grenade_prediction_params::kMaxTicks; ++tick) {
            const float tickStartTime = input.elapsedTime + static_cast<float>(tick) * grenade_prediction_params::kSimDt;
            const float tickEndTime = tickStartTime + grenade_prediction_params::kSimDt;
            if (!Math::isFinite(tickStartTime) || !Math::isFinite(tickEndTime) || tickStartTime >= simulationHorizon())
                break;
            if (pointTimer == 0)
                static_cast<void>(appendPointAtTime(trajectory, position, tickStartTime));
            const auto previousPosition = position;
            const auto result = step(scratch, position, velocity, kind, skipEntity, serverGravity, tickStartTime);
            if (!result.traceSucceeded || !finite(position) || !finite(velocity)) {
                invalidate(trajectory, input.launch.origin);
                return;
            }

            bounceCount += result.contactsCount;
            landedOnSurface = landedOnSurface || result.impactDetonate;
            const bool stopped = (kind == GrenadeKind::SmokeGrenade || kind == GrenadeKind::Decoy)
                && (position - previousPosition).squareLength() < grenade_prediction_params::kStopDisplacementSq;
            if (result.impactDetonate || stopped || bounceCount > grenade_prediction_params::kMaxBounces || shouldDetonateAtElapsedTime(kind, tickStartTime, tickEndTime)) {
                trajectory.endPos = position;
                trajectory.valid = true;
                terminalElapsedTime = tickEndTime;
                if (kind == GrenadeKind::Molotov || kind == GrenadeKind::Incendiary)
                    trajectory.validLanding = landedOnSurface;
                break;
            }
            if (result.hit || ++pointTimer >= grenade_prediction_params::kTicksPerPoint)
                pointTimer = 0;
        }

        if (trajectory.valid && trajectory.pointsCount && trajectory.points[trajectory.pointsCount - 1].squareDistTo(trajectory.endPos) > 1.0f
            && trajectory.pointsCount < Trajectory::kPointsCapacity)
            static_cast<void>(appendPointAtTime(trajectory, trajectory.endPos, terminalElapsedTime));
    }

private:
    [[nodiscard]] static float normalizeThrowStrength(float strength) noexcept
    {
        return strength > 0.4f && strength < 0.6f ? 0.5f : strength;
    }
    [[nodiscard]] static cs2::Vector forwardFromAngles(float pitch, float yaw) noexcept
    {
        float sinePitch, cosinePitch, sineYaw, cosineYaw;
        Math::sincos(pitch * 3.14159265f / 180.0f, sinePitch, cosinePitch);
        Math::sincos(yaw * 3.14159265f / 180.0f, sineYaw, cosineYaw);
        return {cosinePitch * cosineYaw, cosinePitch * sineYaw, -sinePitch};
    }
    struct CollisionResult {
        bool traceSucceeded{true};
        bool impactDetonate{};
        bool stopped{};
    };

    struct SimulationScratch {
        Trajectory* trajectoryOutput;
        const GrenadePlayerCollisionSnapshot* playerCollisionSnapshot;
        bool playerResponseUsed{};
        cs2::CEntityHandle passedPaneHandle{};
        bool hasPassedPane{};
    };

    [[nodiscard]] static bool finite(cs2::Vector value) noexcept { return grenade_player_collision_mirror::finite(value); }
    [[nodiscard]] static float simulationHorizon() noexcept
    {
        return static_cast<float>(grenade_prediction_params::kMaxTicks) * grenade_prediction_params::kSimDt;
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
    }
    [[nodiscard]] Optional<TraceResult> traceGrenadeHull(cs2::Vector start, cs2::Vector end, void* skipEntity) noexcept
    {
        return hookContext.template make<EngineTrace>().traceGrenadeHull(start, end, skipEntity);
    }
    [[nodiscard]] Optional<TraceResult> traceInFlight(const SimulationScratch& scratch, cs2::Vector start, cs2::Vector end, void* skipEntity) noexcept
    {
        if (auto* const passedPane = resolvePassedPane(scratch)) {
            return validateInFlightTrace(scratch, hookContext.template make<EngineTrace>().traceInFlightGrenadeHull(start, end,
                engine_trace::TraceFilterExcludedEntities{skipEntity, passedPane}));
        }
        return validateInFlightTrace(scratch, hookContext.template make<EngineTrace>().traceInFlightGrenadeHull(start, end,
            engine_trace::TraceFilterExcludedEntities{skipEntity, nullptr}));
    }
    [[nodiscard]] static Optional<TraceResult> validateInFlightTrace(const SimulationScratch& scratch, Optional<TraceResult> trace) noexcept
    {
        if (trace.hasValue() && trace.value().fraction < 1.0f
            && (!trace.value().handleRead || (scratch.hasPassedPane && trace.value().rawEntityHandle == static_cast<std::int32_t>(scratch.passedPaneHandle.value))))
            return {};
        return trace;
    }
    [[nodiscard]] StepResult step(SimulationScratch& scratch, cs2::Vector& position, cs2::Vector& velocity, GrenadeKind kind, void* skipEntity,
        float serverGravity) noexcept
    {
        return step(scratch, position, velocity, kind, skipEntity, serverGravity, 0.0f);
    }
    [[nodiscard]] StepResult step(SimulationScratch& scratch, cs2::Vector& position, cs2::Vector& velocity, GrenadeKind kind, void* skipEntity,
        float serverGravity, float elapsedTime) noexcept
    {
        StepResult result;
        if (!scratch.playerResponseUsed && scratch.playerCollisionSnapshot) {
            if (const auto* candidate = grenade_player_collision_mirror::select(*scratch.playerCollisionSnapshot, position);
                candidate && grenade_player_collision_mirror::apply(position, *candidate, velocity)) {
                scratch.playerResponseUsed = true;
                appendPlayerResponsePoint(scratch, position, elapsedTime);
            }
        }
        for (int substep{}; substep < grenade_prediction_params::kMovementSubsteps; ++substep) {
            const auto collision = movementSubstep(scratch, position, velocity, kind, skipEntity, result, serverGravity,
                elapsedTime + static_cast<float>(substep) * grenade_prediction_params::kMovementSubstepDt);
            if (!collision.traceSucceeded)
                return {.traceSucceeded = false};
            if (collision.impactDetonate) {
                result.impactDetonate = true;
                return result;
            }
        }
        return result;
    }
    [[nodiscard]] CollisionResult movementSubstep(SimulationScratch& scratch, cs2::Vector& position, cs2::Vector& velocity, GrenadeKind kind, void* skipEntity,
        StepResult& result, float serverGravity) noexcept
    {
        return movementSubstep(scratch, position, velocity, kind, skipEntity, result, serverGravity, 0.0f);
    }
    [[nodiscard]] CollisionResult movementSubstep(SimulationScratch& scratch, cs2::Vector& position, cs2::Vector& velocity, GrenadeKind kind, void* skipEntity,
        StepResult& result, float serverGravity, float elapsedTime) noexcept
    {
        const float oldZ = velocity.z;
        velocity.z -= serverGravity * grenade_prediction_params::kGravityScale * grenade_prediction_params::kMovementSubstepDt;
        const auto movement = cs2::Vector{velocity.x * grenade_prediction_params::kMovementSubstepDt, velocity.y * grenade_prediction_params::kMovementSubstepDt,
            (oldZ + velocity.z) * 0.5f * grenade_prediction_params::kMovementSubstepDt};
        if (!finite(movement))
            return {.traceSucceeded = false};
        const auto trace = traceInFlight(scratch, position, position + movement, skipEntity);
        if (!validTrace(trace))
            return {.traceSucceeded = false};
        if (trace.value().fraction >= 1.0f) {
            position = position + movement;
            return {};
        }
        cs2::CEntityHandle dynamicPropHandle{};
        if (!scratch.hasPassedPane && getDynamicPropHandle(trace.value(), dynamicPropHandle)) {
            scratch.passedPaneHandle = dynamicPropHandle;
            scratch.hasPassedPane = true;
            position = trace.value().endPos;
            velocity = velocity * 0.4f;
            result.hit = true;
            return continueAfterDynamicProp(scratch, position, velocity, movement, trace.value().fraction, kind, skipEntity, result, elapsedTime);
        }
        if (isUnresolvedNonWorldEntity(trace.value()))
            return {.traceSucceeded = false};
        position = trace.value().endPos;
        result.hit = true;
        ++result.contactsCount;
        appendWorldContactPoint(scratch, position, elapsedTime + trace.value().fraction * grenade_prediction_params::kMovementSubstepDt);
        const auto response = applyContactResponse(trace.value(), velocity, kind);
        if (response.stopped || response.impactDetonate)
            return response;
        const auto remainingTime = (1.0f - trace.value().fraction) * grenade_prediction_params::kMovementSubstepDt;
        const auto continuation = traceInFlight(scratch, position, position + velocity * remainingTime, skipEntity);
        if (!validTrace(continuation))
            return {.traceSucceeded = false};
        if (isUnresolvedNonWorldEntity(continuation.value()))
            return {.traceSucceeded = false};
        position = continuation.value().fraction >= 1.0f ? position + velocity * remainingTime : continuation.value().endPos;
        return {};
    }
    [[nodiscard]] CollisionResult continueAfterDynamicProp(SimulationScratch& scratch, cs2::Vector& position, cs2::Vector& velocity,
        cs2::Vector movement, float impactFraction, GrenadeKind kind, void* skipEntity, StepResult& result, float elapsedTime) noexcept
    {
        const auto continuationMovement = movement * ((1.0f - impactFraction) * 0.4f);
        const auto continuation = traceInFlight(scratch, position, position + continuationMovement, skipEntity);
        if (!validTrace(continuation) || isUnresolvedNonWorldEntity(continuation.value()))
            return {.traceSucceeded = false};
        if (continuation.value().fraction >= 1.0f) {
            position = position + continuationMovement;
            return {};
        }
        position = continuation.value().endPos;
        result.hit = true;
        ++result.contactsCount;
        appendWorldContactPoint(scratch, position, elapsedTime + (impactFraction + (1.0f - impactFraction) * continuation.value().fraction)
            * grenade_prediction_params::kMovementSubstepDt);
        return applyContactResponse(continuation.value(), velocity, kind);
    }
    [[nodiscard]] CollisionResult applyContactResponse(const TraceResult& trace, cs2::Vector& velocity, GrenadeKind kind) noexcept
    {
        auto bounce = clipVelocity(velocity, trace.normal, 2.0f) * grenade_prediction_params::kElasticity;
        const float speedSq = bounce.squareLength();
        if (!finite(bounce) || !Math::isFinite(speedSq))
            return {.traceSucceeded = false};
        if (trace.handleRead && trace.rawEntityHandle == engine_trace::kWorldEntityHandle && trace.normal.z > grenade_prediction_params::kSteepFloorDampingNormalZ
            && speedSq > grenade_prediction_params::kSteepFloorDampingSpeedSq) {
            const float directionDot = (bounce * (1.0f / Math::sqrt(speedSq))).dot(trace.normal);
            if (directionDot > grenade_prediction_params::kSteepFloorDampingDirectionDot)
                bounce = bounce * (grenade_prediction_params::kSteepFloorDampingScaleBase - directionDot);
        }
        if ((kind == GrenadeKind::Molotov || kind == GrenadeKind::Incendiary)
            && (trace.normal.z >= grenade_prediction_params::kMolotovSlope || speedSq < grenade_prediction_params::kStopSpeedSq)) {
            velocity = {};
            return {.impactDetonate = true, .stopped = true};
        }
        if (speedSq < grenade_prediction_params::kStopSpeedSq) {
            velocity = {};
            return {.stopped = true};
        }
        velocity = bounce;
        return {};
    }
    [[nodiscard]] static cs2::Vector clipVelocity(cs2::Vector velocity, cs2::Vector normal, float overbounce) noexcept
    {
        const float projected = -velocity.dot(normal) * overbounce;
        const float backoff = (projected > 0.0f ? projected : 0.0f) + grenade_prediction_params::kClipPushOff;
        return velocity + normal * backoff;
    }
    [[nodiscard]] static bool appendPointAtTime(Trajectory& trajectory, cs2::Vector point, float elapsedTime) noexcept
    {
        if (!finite(point) || !Math::isFinite(elapsedTime))
            return false;
        if (trajectory.pointsCount) {
            const float previousElapsedTime = trajectory.elapsedTimes[trajectory.pointsCount - 1];
            if (elapsedTime == previousElapsedTime)
                return true;
            if (elapsedTime < previousElapsedTime)
                return false;
        }
        return trajectory.appendPoint(point, elapsedTime);
    }
    void appendWorldContactPoint(const SimulationScratch& scratch, cs2::Vector point, float elapsedTime) noexcept
    {
        if (scratch.trajectoryOutput && appendPointAtTime(*scratch.trajectoryOutput, point, elapsedTime))
            static_cast<void>(scratch.trajectoryOutput->appendWorldContactMarker());
    }
    void appendPlayerResponsePoint(const SimulationScratch& scratch, cs2::Vector point, float elapsedTime) noexcept
    {
        if (scratch.trajectoryOutput && appendPointAtTime(*scratch.trajectoryOutput, point, elapsedTime))
            static_cast<void>(scratch.trajectoryOutput->appendPlayerResponseMarker());
    }
    [[nodiscard]] bool getDynamicPropHandle(const TraceResult& traceResult, cs2::CEntityHandle& dynamicPropHandle) const noexcept
    {
        if constexpr (!requires(HookContext& context, cs2::CEntityHandle handle) {
            context.template make<EntitySystem>().getEntityFromHandle(handle);
            context.entityClassifier().template entityIs<cs2::C_DynamicProp>(nullptr);
        }) return false;
        else {
            if (!traceResult.handleRead)
                return false;
            const cs2::CEntityHandle handle{static_cast<std::uint32_t>(traceResult.rawEntityHandle)};
            const auto entitySystem = hookContext.template make<EntitySystem>();
            auto* const entity = entitySystem.getEntityFromHandle(handle);
            if (!entity || !entity->identity || entity->identity->entity != entity || entity->identity->handle != handle)
                return false;
            if (!hookContext.entityClassifier().template entityIs<cs2::C_DynamicProp>(entity->identity->entityClass))
                return false;
            dynamicPropHandle = handle;
            return true;
        }
    }
    [[nodiscard]] bool isUnresolvedNonWorldEntity(const TraceResult& traceResult) const noexcept
    {
        if (traceResult.fraction >= 1.0f || !traceResult.handleRead || traceResult.rawEntityHandle == engine_trace::kWorldEntityHandle)
            return false;
        if constexpr (!requires(HookContext& context, cs2::CEntityHandle handle) { context.template make<EntitySystem>().getEntityFromHandle(handle); })
            return true;
        else {
            const cs2::CEntityHandle handle{static_cast<std::uint32_t>(traceResult.rawEntityHandle)};
            const auto entitySystem = hookContext.template make<EntitySystem>();
            const auto* const entity = entitySystem.getEntityFromHandle(handle);
            return !entity || !entity->identity || entity->identity->entity != entity || entity->identity->handle != handle || !entity->identity->entityClass;
        }
    }
    [[nodiscard]] void* resolvePassedPane(const SimulationScratch& scratch) const noexcept
    {
        if (!scratch.hasPassedPane)
            return nullptr;
        if constexpr (!requires(HookContext& context, cs2::CEntityHandle handle) { context.template make<EntitySystem>().getEntityFromHandle(handle); })
            return nullptr;
        else {
            const auto entitySystem = hookContext.template make<EntitySystem>();
            auto* const entity = entitySystem.getEntityFromHandle(scratch.passedPaneHandle);
            return entity && entity->identity && entity->identity->entity == entity && entity->identity->handle == scratch.passedPaneHandle ? entity : nullptr;
        }
    }
    [[nodiscard]] static bool shouldDetonate(GrenadeKind kind, int tick) noexcept
    {
        const float elapsed = static_cast<float>(tick + 1) * grenade_prediction_params::kSimDt;
        switch (kind) {
        case GrenadeKind::Flashbang:
        case GrenadeKind::HEGrenade:
            return elapsed > grenade_prediction_params::kDetonateTimeHeFlash + grenade_prediction_params::kClientTracerHorizonPadding;
        case GrenadeKind::Molotov:
        case GrenadeKind::Incendiary:
            return elapsed > grenade_prediction_params::kDetonateTimeMolotov + grenade_prediction_params::kClientTracerHorizonPadding;
        case GrenadeKind::Decoy:
            return static_cast<float>(tick) * grenade_prediction_params::kSimDt > grenade_prediction_params::kDetonateTimeDecoy;
        case GrenadeKind::SmokeGrenade:
            return static_cast<float>(tick) * grenade_prediction_params::kSimDt > grenade_prediction_params::kDetonateTimeSmokeCap;
        default:
            return false;
        }
    }
    [[nodiscard]] static bool shouldDetonateAtElapsedTime(GrenadeKind kind, float tickStartTime, float tickEndTime) noexcept
    {
        switch (kind) {
        case GrenadeKind::Flashbang:
        case GrenadeKind::HEGrenade:
            return tickEndTime > grenade_prediction_params::kDetonateTimeHeFlash + grenade_prediction_params::kClientTracerHorizonPadding;
        case GrenadeKind::Molotov:
        case GrenadeKind::Incendiary:
            return tickEndTime > grenade_prediction_params::kDetonateTimeMolotov + grenade_prediction_params::kClientTracerHorizonPadding;
        case GrenadeKind::Decoy:
            return tickStartTime > grenade_prediction_params::kDetonateTimeDecoy;
        case GrenadeKind::SmokeGrenade:
            return tickStartTime > grenade_prediction_params::kDetonateTimeSmokeCap;
        default:
            return false;
        }
    }

    HookContext& hookContext;
    const GrenadePlayerCollisionSnapshot* configuredPlayerCollisionSnapshot{};
    friend struct GrenadeSimulatorTestAccess<HookContext>;
};
