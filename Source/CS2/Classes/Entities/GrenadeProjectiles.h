#pragma once

#include <cstdint>

#include "C_BaseCSGrenadeProjectile.h"

namespace cs2
{

struct C_HEGrenadeProjectile : C_BaseCSGrenadeProjectile {
    using m_nExplodeEffectTickBegin = std::int32_t;
};

struct C_SmokeGrenadeProjectile : C_BaseCSGrenadeProjectile {
    using m_bDidSmokeEffect = bool;
};

struct C_MolotovProjectile : C_BaseCSGrenadeProjectile {
};

struct C_FlashbangProjectile : C_BaseCSGrenadeProjectile {
};

struct C_DecoyProjectile : C_BaseCSGrenadeProjectile {
    using m_nDecoyShotTick = int;
};

}
