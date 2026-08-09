#pragma once

#include <cstdint>

#include <Features/Visuals/GrenadePrediction/GrenadeKind.h>
#include <Utils/Optional.h>

enum class LiveGrenadeLifecycle { Keep, Remove };

struct LiveGrenadeLifecycleState {
    Optional<bool> smokeEffectStarted;
    Optional<std::int32_t> heExplodeEffectTickBegin;
    Optional<std::int32_t> decoyShotTick;
};

[[nodiscard]] inline LiveGrenadeLifecycle getLiveGrenadeLifecycle(cs2::GrenadeKind kind, const LiveGrenadeLifecycleState& state) noexcept
{
    switch (kind) {
    case cs2::GrenadeKind::HEGrenade:
        return state.heExplodeEffectTickBegin.greaterThan(0).valueOr(false) ? LiveGrenadeLifecycle::Remove : LiveGrenadeLifecycle::Keep;
    case cs2::GrenadeKind::SmokeGrenade:
        return state.smokeEffectStarted.valueOr(false) ? LiveGrenadeLifecycle::Remove : LiveGrenadeLifecycle::Keep;
    case cs2::GrenadeKind::Decoy:
        return state.decoyShotTick.greaterThan(0).valueOr(false) ? LiveGrenadeLifecycle::Remove : LiveGrenadeLifecycle::Keep;
    default:
        return LiveGrenadeLifecycle::Keep;
    }
}
