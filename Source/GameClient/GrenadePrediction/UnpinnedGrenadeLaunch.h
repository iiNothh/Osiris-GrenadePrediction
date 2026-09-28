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

#include "GrenadeLaunchState.h"

namespace unpinned_grenade_launch_params
{
    constexpr float kBaseVelocity{750.0f};
    constexpr float kStrength{1.0f};
    constexpr float kMinimumVelocity{15.0f};
    constexpr float kVelocityScale{0.9f};
    constexpr float kMinimumStrengthScale{0.3f};
    constexpr float kStrengthVelocityScale{0.7f};
    constexpr float kPitchWrap{360.0f};
    constexpr float kSourceZAdjustment{12.0f};
    constexpr float kEndpointTraceForward{16.0f};
    constexpr float kMovementVelocityScale{1.25f};
}

template <typename HookContext>
class UnpinnedGrenadeLaunch {
public:
    explicit UnpinnedGrenadeLaunch(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    [[nodiscard]] Optional<GrenadeLaunchState> get(cs2::C_BaseCSGrenade* grenade, cs2::C_CSPlayerPawn* pawn) const noexcept
    {
        if constexpr (!GrenadePredictionPlatformCapabilities::supportsNativeUnpinnedHeldLaunch) {
            return {};
        } else {
            if (grenade == nullptr || pawn == nullptr || pawn->identity == nullptr)
                return {};

            const auto& results = hookContext.patternSearchResults();
            const auto gatherInputs = results.template get<GatherGrenadeLaunchInputsFunction>();
            const auto owner = results.template get<OffsetToOwnerEntity>().of(static_cast<cs2::C_BaseEntity*>(grenade)).toOptional();
            const auto pawnHandle = pawn->identity->handle;
            if (gatherInputs == nullptr || !owner.hasValue() || owner.value() == cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}
                || pawnHandle == cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX} || owner.value() != pawnHandle)
                return {};

            const float nonFinite = std::bit_cast<float>(std::uint32_t{0x7FC00000u});
            cs2::Vector source{nonFinite, nonFinite, nonFinite};
            cs2::Vector viewAngles{nonFinite, nonFinite, nonFinite};
            cs2::Vector movement{nonFinite, nonFinite, nonFinite};
            cs2::Vector center{nonFinite, nonFinite, nonFinite};
            static_cast<void>(gatherInputs(pawn, &viewAngles, &source, &center, &movement, false));
            if (!source.isFinite() || !viewAngles.isFinite() || !movement.isFinite() || !center.isFinite())
                return {};

            const auto collisionSphere = hookContext.cvarSystem().template getConVarValue<cs2::sv_grenade_collision_sphere>();
            if (!collisionSphere.has_value())
                return {};

            const auto launch = finalize(source, viewAngles, movement);
            if (!launch.hasValue())
                return {};
            if (collisionSphere.value())
                return launch;

            const auto forward = forwardFromViewAngles(viewAngles);
            if (!forward.isFinite())
                return {};
            const auto endpoint = engine_trace::grenade::traceLaunchEndpoint(hookContext, center,
                launch.value().origin + forward * unpinned_grenade_launch_params::kEndpointTraceForward, pawn);
            if (!endpoint.hasValue())
                return {};
            return GrenadeLaunchState{endpoint.value(), launch.value().velocity};
        }
    }

    [[nodiscard]] static Optional<GrenadeLaunchState> finalize(cs2::Vector source, cs2::Vector viewAngles, cs2::Vector movement) noexcept
    {
        const float speed = std::clamp(unpinned_grenade_launch_params::kBaseVelocity * unpinned_grenade_launch_params::kVelocityScale,
            unpinned_grenade_launch_params::kMinimumVelocity, unpinned_grenade_launch_params::kBaseVelocity)
            * (unpinned_grenade_launch_params::kMinimumStrengthScale + unpinned_grenade_launch_params::kStrengthVelocityScale * unpinned_grenade_launch_params::kStrength);
        const auto forward = forwardFromViewAngles(viewAngles);
        source.z += unpinned_grenade_launch_params::kStrength * unpinned_grenade_launch_params::kSourceZAdjustment - unpinned_grenade_launch_params::kSourceZAdjustment;
        const auto velocity = forward * speed + movement * unpinned_grenade_launch_params::kMovementVelocityScale;
        return source.isFinite() && forward.isFinite() && velocity.isFinite() ? Optional<GrenadeLaunchState>{GrenadeLaunchState{source, velocity}} : Optional<GrenadeLaunchState>{};
    }

private:
    [[nodiscard]] static cs2::Vector forwardFromViewAngles(cs2::Vector viewAngles) noexcept
    {
        float pitch = viewAngles.x;
        if (pitch > 90.0f)
            pitch -= unpinned_grenade_launch_params::kPitchWrap;
        else if (pitch < -90.0f)
            pitch += unpinned_grenade_launch_params::kPitchWrap;
        pitch -= (90.0f - Math::abs(pitch)) * 10.0f / 90.0f;

        float sinePitch, cosinePitch, sineYaw, cosineYaw;
        Math::sincos(pitch * 3.14159265f / 180.0f, sinePitch, cosinePitch);
        Math::sincos(viewAngles.y * 3.14159265f / 180.0f, sineYaw, cosineYaw);
        return {cosinePitch * cosineYaw, cosinePitch * sineYaw, -sinePitch};
    }

    HookContext& hookContext;
};
