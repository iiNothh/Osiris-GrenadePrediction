#pragma once

#include <cstdint>

#include <CS2/Classes/EntitySystem/CEntityHandle.h>
#include <Features/Visuals/GrenadePrediction/Rendering/GrenadePredictionRendererState.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeSimulationResult.h>

namespace grenade_prediction
{

struct LiveGrenadeAuthorityState {
    GrenadeSimulationResult candidateSimulationResult;
    GrenadeTrajectory acceptedTrajectory;
    GrenadePredictionRendererState rendererState;
    cs2::CEntityHandle acceptedProjectileHandle{};
    cs2::CEntityHandle acceptedThrowerHandle{};
    std::uint64_t acceptedFirstObservationSequence{};
    cs2::CEntityHandle localPawnIdentity{};
    std::uint64_t watermark{};
    bool accepted{};
};

}
