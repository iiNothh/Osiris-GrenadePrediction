#pragma once

#include <CS2/Classes/Vector.h>

namespace grenade_prediction
{

struct GrenadeLaunchState {
    cs2::Vector origin{};
    cs2::Vector velocity{};
};

}
