#pragma once

#include <cstdint>

#include <CS2/Classes/Vector.h>

#include "C_BaseModelEntity.h"

namespace cs2
{

struct C_Inferno : C_BaseModelEntity {
    using m_firePositions = Vector[64];
    using m_bFireIsBurning = bool[64];
    using m_fireCount = std::int32_t;
    using m_nFireLifetime = float;
};

}
