#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

#include <CS2/Classes/ConVarTypes.h>
#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Classes/Entities/WeaponEntities.h>
#include <CS2/Classes/EntitySystem/CEntityIdentity.h>
#include <CS2/Constants/EntityHandle.h>
#include <GameClient/EngineTrace/GrenadeTrace.h>
#include <MemoryPatterns/PatternTypes/ClientPatternTypes.h>
#include <MemoryPatterns/PatternTypes/EntityPatternTypes.h>
#include <Platform/GrenadePredictionCapabilities.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

#include "GatheredGrenadeLaunchParams.h"
#include "GrenadeLaunchState.h"

struct GatheredGrenadeLaunchInputs {
    [[nodiscard]] static constexpr cs2::Vector makeNonFiniteVector() noexcept
    {
        const float nonFinite = std::bit_cast<float>(std::uint32_t{0x7FC00000u});
        return {nonFinite, nonFinite, nonFinite};
    }

    cs2::Vector viewAngles{makeNonFiniteVector()};
    cs2::Vector source{makeNonFiniteVector()};
    cs2::Vector center{makeNonFiniteVector()};
    cs2::Vector movement{makeNonFiniteVector()};

    [[nodiscard]] bool isFinite() const noexcept
    {
        return viewAngles.isFinite() && source.isFinite() && center.isFinite() && movement.isFinite();
    }
};

template <typename HookContext>
class GatheredGrenadeLaunch {
public:
    explicit GatheredGrenadeLaunch(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    [[nodiscard]] Optional<GrenadeLaunchState> get(cs2::C_BaseCSGrenade* grenade, cs2::C_CSPlayerPawn* pawn, float throwStrength) const noexcept
    {
        if constexpr (GrenadePredictionPlatformCapabilities::supportsGatheredHeldLaunch) {
            if (!hasValidGrenadeAndPawn(grenade, pawn))
                return {};

            const auto& results = hookContext.patternSearchResults();
            const auto gatherInputs = results.template get<GatherGrenadeLaunchInputsFunction>();
            const auto owner = results.template get<OffsetToOwnerEntity>().of(static_cast<cs2::C_BaseEntity*>(grenade)).toOptional();
            const auto pawnHandle = pawn->identity->handle;
            if (gatherInputs == nullptr || !owner.hasValue() || owner.value() == cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}
                || pawnHandle == cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX} || owner.value() != pawnHandle)
                return {};

            GatheredGrenadeLaunchInputs inputs;
            static_cast<void>(gatherInputs(pawn, &inputs.viewAngles, &inputs.source, &inputs.center, &inputs.movement, false));
            if (!inputs.isFinite())
                return {};

            return resolveCollisionSphereLaunch(inputs, throwStrength, pawn);
        } else {
            return {};
        }
    }

    [[nodiscard]] static Optional<GrenadeLaunchState> finalize(cs2::Vector source, cs2::Vector viewAngles, cs2::Vector movement, float throwStrength) noexcept
    {
        const auto finalizedLaunch = finalizeWithForward(source, viewAngles, movement, throwStrength);
        return finalizedLaunch.hasValue() ? Optional<GrenadeLaunchState>{finalizedLaunch.value().launch} : Optional<GrenadeLaunchState>{};
    }

private:
    struct FinalizedGrenadeLaunch {
        GrenadeLaunchState launch;
        cs2::Vector forward;
    };

    [[nodiscard]] static Optional<FinalizedGrenadeLaunch> finalizeWithForward(cs2::Vector source, cs2::Vector viewAngles, cs2::Vector movement,
        float throwStrength) noexcept
    {
        using namespace gathered_grenade_launch_params;

        const float speed =
            std::clamp(kBaseVelocity * kVelocityScale, kMinimumVelocity, kBaseVelocity)
            * (kMinimumStrengthScale + kStrengthVelocityScale * throwStrength);

        const auto forward = forwardFromViewAngles(viewAngles);

        source.z += throwStrength * kSourceZAdjustment - kSourceZAdjustment;

        const auto velocity = forward * speed + movement * kMovementVelocityScale;

        return source.isFinite() && forward.isFinite() && velocity.isFinite()
            ? Optional<FinalizedGrenadeLaunch>{FinalizedGrenadeLaunch{{source, velocity}, forward}}
            : Optional<FinalizedGrenadeLaunch>{};
    }

    [[nodiscard]] static bool hasValidGrenadeAndPawn(const cs2::C_BaseCSGrenade* grenade, const cs2::C_CSPlayerPawn* pawn) noexcept
    {
        return grenade != nullptr && pawn != nullptr && grenade->identity != nullptr && pawn->identity != nullptr
            && grenade->identity->entity == grenade && pawn->identity->entity == pawn;
    }

    [[nodiscard]] Optional<GrenadeLaunchState> resolveCollisionSphereLaunch(const GatheredGrenadeLaunchInputs& inputs, float throwStrength, cs2::C_CSPlayerPawn* pawn) const noexcept
    {
        const auto collisionSphere = hookContext.cvarSystem().template getConVarValue<cs2::sv_grenade_collision_sphere>();
        if (!collisionSphere.has_value())
            return {};

        const auto finalizedLaunch = finalizeWithForward(inputs.source, inputs.viewAngles, inputs.movement, throwStrength);
        if (!finalizedLaunch.hasValue())
            return {};
        const auto& launch = finalizedLaunch.value().launch;
        if (*collisionSphere)
            return launch;

        const auto endpoint = engine_trace::grenade::traceLaunchEndpoint(hookContext, inputs.center,
            launch.origin + finalizedLaunch.value().forward * gathered_grenade_launch_params::kEndpointTraceForward, pawn);
        if (!endpoint.hasValue())
            return {};
        return GrenadeLaunchState{endpoint.value(), launch.velocity};
    }

    [[nodiscard]] static cs2::Vector forwardFromViewAngles(cs2::Vector viewAngles) noexcept
    {
        using namespace gathered_grenade_launch_params;

        float pitch = viewAngles.x;
        if (pitch > 90.0f)
            pitch -= kPitchWrap;
        else if (pitch < -90.0f)
            pitch += kPitchWrap;
        pitch -= (90.0f - Math::abs(pitch)) * 10.0f / 90.0f;

        float sinePitch, cosinePitch, sineYaw, cosineYaw;
        Math::sincos(viewAngles.y * kDegreesToRadians, sineYaw, cosineYaw);
        Math::sincos(pitch * kDegreesToRadians, sinePitch, cosinePitch);
        return {cosinePitch * cosineYaw, cosinePitch * sineYaw, -sinePitch};
    }

    HookContext& hookContext;
};
