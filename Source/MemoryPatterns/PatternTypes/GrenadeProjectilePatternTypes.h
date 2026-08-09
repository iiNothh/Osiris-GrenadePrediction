#pragma once

#include <cstdint>

#include <CS2/Classes/Entities/C_BaseCSGrenadeProjectile.h>
#include <CS2/Classes/Entities/C_BaseGrenade.h>
#include <Utils/FieldOffset.h>
#include <Utils/StrongTypeAlias.h>

template <typename FieldType, typename OffsetType>
using GrenadeProjectileOffset = FieldOffset<cs2::C_BaseCSGrenadeProjectile, FieldType, OffsetType>;
template <typename FieldType, typename OffsetType>
using GrenadeOffset = FieldOffset<cs2::C_BaseGrenade, FieldType, OffsetType>;

STRONG_TYPE_ALIAS(OffsetToGrenadeInitialPosition, GrenadeProjectileOffset<cs2::C_BaseCSGrenadeProjectile::m_vInitialPosition, std::int32_t>);
STRONG_TYPE_ALIAS(OffsetToGrenadeInitialVelocity, GrenadeProjectileOffset<cs2::C_BaseCSGrenadeProjectile::m_vInitialVelocity, std::int32_t>);
STRONG_TYPE_ALIAS(OffsetToGrenadeThrower, GrenadeOffset<cs2::C_BaseGrenade::m_hThrower, std::int32_t>);
STRONG_TYPE_ALIAS(OffsetToExplodeEffectTickBegin, GrenadeProjectileOffset<cs2::C_BaseCSGrenadeProjectile::m_nExplodeEffectTickBegin, std::int32_t>);
