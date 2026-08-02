#pragma once

#include <cstddef>

namespace grenade_prediction
{

struct GrenadeSimulationParameters {
    float tickInterval{1.0f / 64.0f};
    float gravity{800.0f};
    float gravityScale{0.4f};
    float elasticity{0.45f};
    float contactPushOff{0.03125f};
    float stopSpeedSquared{400.0f};
    float molotovSlopeNormalZ{0.8660254f};
    float stoppedDisplacementSquared{1.42e-14f};
    float heAndFlashDetonationTime{1.625f};
    float molotovDetonationTime{2.125f};
    float decoyDetonationTime{10.0f};
    float smokeDetonationTime{18.0f};
    std::size_t movementSubsteps{2};
    std::size_t pointIntervalTicks{2};
    std::size_t maximumTicks{1154};
    std::size_t maximumBounces{20};
};

}
