#pragma once

#include <Features/Visuals/GrenadePrediction/Held/HeldGrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeAuthorityState.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheState.h>

struct GrenadePredictionState {
    grenade_prediction::HeldGrenadePredictionState held;
    grenade_prediction::LiveGrenadeCacheState liveCache;
    grenade_prediction::LiveGrenadeAuthorityState liveAuthority;
};
