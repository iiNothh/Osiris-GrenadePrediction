#pragma once

#include <cstdint>

enum class GrenadeKind : std::uint8_t {
    None,
    Flashbang,
    HEGrenade,
    SmokeGrenade,
    Molotov,
    Decoy,
    Incendiary
};
