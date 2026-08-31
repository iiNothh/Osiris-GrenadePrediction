#pragma once

#include <GameClient/Entities/GrenadeKind.h>
#include <GameClient/Entities/EntityClassifier.h>

struct GrenadeKindMapper {
    [[nodiscard]] static constexpr GrenadeKind from(EntityTypeInfo entityTypeInfo) noexcept
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

    [[nodiscard]] static constexpr GrenadeKind fromProjectile(EntityTypeInfo entityTypeInfo) noexcept
    {
        switch (entityTypeInfo.typeIndex) {
        case EntityTypeInfo::indexOf<cs2::C_HEGrenadeProjectile>(): return GrenadeKind::HEGrenade;
        case EntityTypeInfo::indexOf<cs2::C_SmokeGrenadeProjectile>(): return GrenadeKind::SmokeGrenade;
        case EntityTypeInfo::indexOf<cs2::C_MolotovProjectile>(): return GrenadeKind::Molotov;
        case EntityTypeInfo::indexOf<cs2::C_FlashbangProjectile>(): return GrenadeKind::Flashbang;
        case EntityTypeInfo::indexOf<cs2::C_DecoyProjectile>(): return GrenadeKind::Decoy;
        default: return GrenadeKind::None;
        }
    }
};
