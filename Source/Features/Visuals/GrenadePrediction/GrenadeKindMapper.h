#pragma once

#include <Features/Visuals/GrenadePrediction/GrenadeKind.h>
#include <GameClient/Entities/EntityClassifier.h>

struct GrenadeKindMapper {
    [[nodiscard]] static constexpr cs2::GrenadeKind from(EntityTypeInfo entityTypeInfo) noexcept
    {
        switch (entityTypeInfo.typeIndex) {
        case EntityTypeInfo::indexOf<cs2::C_Flashbang>(): return cs2::GrenadeKind::Flashbang;
        case EntityTypeInfo::indexOf<cs2::C_HEGrenade>(): return cs2::GrenadeKind::HEGrenade;
        case EntityTypeInfo::indexOf<cs2::C_SmokeGrenade>(): return cs2::GrenadeKind::SmokeGrenade;
        case EntityTypeInfo::indexOf<cs2::C_MolotovGrenade>(): return cs2::GrenadeKind::Molotov;
        case EntityTypeInfo::indexOf<cs2::C_IncendiaryGrenade>(): return cs2::GrenadeKind::Incendiary;
        case EntityTypeInfo::indexOf<cs2::C_DecoyGrenade>(): return cs2::GrenadeKind::Decoy;
        default: return cs2::GrenadeKind::None;
        }
    }
};
