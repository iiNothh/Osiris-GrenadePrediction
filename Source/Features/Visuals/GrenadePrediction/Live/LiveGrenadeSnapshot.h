#pragma once

#include <cstdint>

#include <CS2/Classes/EntitySystem/CEntityHandle.h>
#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Classes/Vector.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeKind.h>

namespace grenade_prediction
{

struct LiveGrenadeSnapshot {
    cs2::CEntityHandle handle{};
    cs2::CHandle<cs2::C_CSPlayerPawn> thrower{};
    cs2::Vector initialPosition{};
    cs2::Vector initialVelocity{};
    GrenadeKind kind{GrenadeKind::None};
    std::uint64_t firstObservationSequence{};
    bool seen{};
};

}
