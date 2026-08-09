#pragma once

#include <cstdint>

#include <CS2/Classes/Vector.h>

#include "C_BaseGrenade.h"

namespace cs2
{

struct C_BaseCSGrenadeProjectile : C_BaseGrenade {
    using m_vInitialPosition = Vector;
    using m_vInitialVelocity = Vector;
    using m_nExplodeEffectTickBegin = std::int32_t;
};

}
