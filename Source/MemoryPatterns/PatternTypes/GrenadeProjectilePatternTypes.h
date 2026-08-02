#pragma once

#include <cstdint>

#include <CS2/Classes/Entities/C_BaseCSGrenadeProjectile.h>
#include <CS2/Classes/Entities/C_BaseGrenade.h>
#include <Utils/FieldOffset.h>
#include <Utils/StrongTypeAlias.h>

template <typename FieldType>
using GrenadeProjectileOffset = FieldOffset<cs2::C_BaseCSGrenadeProjectile, FieldType, std::int32_t>;

STRONG_TYPE_ALIAS(OffsetToInitialPosition, GrenadeProjectileOffset<cs2::C_BaseCSGrenadeProjectile::m_vInitialPosition>);
STRONG_TYPE_ALIAS(OffsetToInitialVelocity, GrenadeProjectileOffset<cs2::C_BaseCSGrenadeProjectile::m_vInitialVelocity>);
STRONG_TYPE_ALIAS(OffsetToThrower, FieldOffset<cs2::C_BaseGrenade, cs2::C_BaseGrenade::m_hThrower, std::int32_t>);
