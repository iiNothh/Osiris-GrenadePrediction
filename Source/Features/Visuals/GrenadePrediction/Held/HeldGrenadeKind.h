#pragma once

#include <CS2/Classes/Entities/GrenadeProjectiles.h>
#include <CS2/Classes/Entities/WeaponEntities.h>
#include <GameClient/Entities/EntityClassifier.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeKind.h>

namespace grenade_prediction
{

[[nodiscard]] constexpr GrenadeKind heldGrenadeKind(EntityTypeInfo entityTypeInfo) noexcept
{
    switch (entityTypeInfo.typeIndex) {
    case EntityTypeInfo::indexOf<cs2::C_Flashbang>(): return GrenadeKind::Flashbang;
    case EntityTypeInfo::indexOf<cs2::C_HEGrenade>(): return GrenadeKind::HEGrenade;
    case EntityTypeInfo::indexOf<cs2::C_SmokeGrenade>(): return GrenadeKind::SmokeGrenade;
    case EntityTypeInfo::indexOf<cs2::C_MolotovGrenade>(): return GrenadeKind::Molotov;
    case EntityTypeInfo::indexOf<cs2::C_IncendiaryGrenade>(): return GrenadeKind::Incendiary;
    case EntityTypeInfo::indexOf<cs2::C_DecoyGrenade>(): return GrenadeKind::Decoy;
    default: return GrenadeKind::None;
    }
}

}
