#pragma once

#include <Features/Visuals/GrenadePrediction/Rendering/GrenadePredictionRendererState.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeSimulationResult.h>

namespace grenade_prediction
{

struct HeldGrenadePredictionState {
    GrenadeSimulationResult simulationResult;
    GrenadePredictionRendererState rendererState;
};

}
