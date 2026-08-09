#pragma once

#include <cstdint>

#include <CS2/Classes/Entities/GrenadeProjectiles.h>
#include <Utils/FieldOffset.h>
#include <Utils/StrongTypeAlias.h>

template <typename FieldType, typename OffsetType>
using DecoyProjectileOffset = FieldOffset<cs2::C_DecoyProjectile, FieldType, OffsetType>;

STRONG_TYPE_ALIAS(OffsetToDecoyShotTick, DecoyProjectileOffset<cs2::C_DecoyProjectile::m_nDecoyShotTick, std::int32_t>);
