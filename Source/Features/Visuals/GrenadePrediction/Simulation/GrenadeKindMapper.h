#pragma once

#include <CS2/Econ/ItemDefinitionIndex.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeKind.h>

namespace grenade_prediction
{

[[nodiscard]] constexpr GrenadeKind grenadeKindFromItemDefinition(cs2::ItemDefinitionIndex itemDefinitionIndex) noexcept
{
    switch (itemDefinitionIndex) {
    case cs2::ItemDefinitionIndex::Flashbang: return GrenadeKind::Flashbang;
    case cs2::ItemDefinitionIndex::HEGrenade: return GrenadeKind::HEGrenade;
    case cs2::ItemDefinitionIndex::SmokeGrenade: return GrenadeKind::SmokeGrenade;
    case cs2::ItemDefinitionIndex::Molotov: return GrenadeKind::Molotov;
    case cs2::ItemDefinitionIndex::Decoy: return GrenadeKind::Decoy;
    case cs2::ItemDefinitionIndex::Incendiary: return GrenadeKind::Incendiary;
    default: return GrenadeKind::None;
    }
}

}
