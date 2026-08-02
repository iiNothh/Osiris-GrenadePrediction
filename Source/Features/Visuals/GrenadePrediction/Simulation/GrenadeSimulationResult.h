#pragma once

#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeTrajectory.h>

namespace grenade_prediction
{

struct GrenadeSimulationResult {
    GrenadeTrajectory trajectory{};
    bool traceFailed{};
    bool detonated{};
};

}
