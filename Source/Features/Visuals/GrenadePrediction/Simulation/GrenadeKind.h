#pragma once

#include <cstdint>

namespace grenade_prediction
{

enum class GrenadeKind : std::uint8_t {
    None,
    Flashbang,
    HEGrenade,
    SmokeGrenade,
    Molotov,
    Decoy,
    Incendiary
};

}
