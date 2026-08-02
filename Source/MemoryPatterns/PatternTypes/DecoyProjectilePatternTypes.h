#pragma once

#include <cstdint>

#include <CS2/Classes/Entities/GrenadeProjectiles.h>
#include <Utils/FieldOffset.h>
#include <Utils/StrongTypeAlias.h>

STRONG_TYPE_ALIAS(OffsetToDecoyShotTick, FieldOffset<cs2::C_DecoyProjectile, cs2::C_DecoyProjectile::m_nDecoyShotTick, std::int32_t>);
