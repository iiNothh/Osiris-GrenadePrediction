#pragma once

#include <cstdint>

namespace cs2::engine_trace {

    enum class InteractionLayer : std::uint64_t {
        Empty = 0,
        Solid = std::uint64_t{1} << 0,
        Hitboxes = std::uint64_t{1} << 1,
        Trigger = std::uint64_t{1} << 2,
        Sky = std::uint64_t{1} << 3,
        PlayerClip = std::uint64_t{1} << 4,
        NpcClip = std::uint64_t{1} << 5,
        BlockLos = std::uint64_t{1} << 6,
        BlockLight = std::uint64_t{1} << 7,
        Ladder = std::uint64_t{1} << 8,
        Pickup = std::uint64_t{1} << 9,
        BlockSound = std::uint64_t{1} << 10,
        NoDraw = std::uint64_t{1} << 11,
        Window = std::uint64_t{1} << 12,
        PassBullets = std::uint64_t{1} << 13,
        WorldGeometry = std::uint64_t{1} << 14,
        Water = std::uint64_t{1} << 15,
        Slime = std::uint64_t{1} << 16,
        TouchAll = std::uint64_t{1} << 17,
        Player = std::uint64_t{1} << 18,
        Npc = std::uint64_t{1} << 19,
        Debris = std::uint64_t{1} << 20,
        PhysicsProp = std::uint64_t{1} << 21,
        NavIgnore = std::uint64_t{1} << 22,
        NavLocalIgnore = std::uint64_t{1} << 23,
        PostProcessingVolume = std::uint64_t{1} << 24,
        VehicleClip = std::uint64_t{1} << 25,
        CarriedObject = std::uint64_t{1} << 26,
        PushAway = std::uint64_t{1} << 27,
        ServerEntityOnClient = std::uint64_t{1} << 28,
        CarriedWeapon = std::uint64_t{1} << 29,
        StaticLevel = std::uint64_t{1} << 30,
        CsgoTeam1 = std::uint64_t{1} << 31,
        CsgoTeam2 = std::uint64_t{1} << 32,
        CsgoGrenadeClip = std::uint64_t{1} << 33,
        CsgoDroneClip = std::uint64_t{1} << 34,
        CsgoMoveable = std::uint64_t{1} << 35,
        CsgoOpaque = std::uint64_t{1} << 36,
        CsgoMonster = std::uint64_t{1} << 37,
        CsgoChicken = std::uint64_t{1} << 38,
        CsgoThrownGrenade = std::uint64_t{1} << 39,
        CsgoDroppedHostage = std::uint64_t{1} << 40
    };

    [[nodiscard]] constexpr InteractionLayer operator|(InteractionLayer lhs, InteractionLayer rhs) noexcept
    {
        return static_cast<InteractionLayer>(static_cast<std::uint64_t>(lhs) | static_cast<std::uint64_t>(rhs));
    }

    [[nodiscard]] constexpr InteractionLayer operator&(InteractionLayer lhs, InteractionLayer rhs) noexcept
    {
        return static_cast<InteractionLayer>(static_cast<std::uint64_t>(lhs) & static_cast<std::uint64_t>(rhs));
    }

    [[nodiscard]] constexpr InteractionLayer operator~(InteractionLayer interactionLayer) noexcept
    {
        return static_cast<InteractionLayer>(~static_cast<std::uint64_t>(interactionLayer));
    }

    constexpr auto kSolidMask = InteractionLayer::Solid | InteractionLayer::BlockLos | InteractionLayer::BlockLight | InteractionLayer::BlockSound;
    constexpr auto kSolidWithoutBlockLosMask = InteractionLayer::Solid | InteractionLayer::BlockLight | InteractionLayer::BlockSound;
}

// Observed game-object interaction layers:
// Wall: Solid | BlockLos | BlockLight | BlockSound | StaticLevel
// Player clip: PlayerClip | NpcClip | StaticLevel
// Window dynamic prop: BlockSound | Window | TouchAll | ServerEntityOnClient
// Railing: PassBullets | StaticLevel
// Interactive door: Solid | BlockLight | BlockSound | TouchAll | PushAway | ServerEntityOnClient
// Vent cover: Solid | BlockLos | BlockLight | TouchAll | ServerEntityOnClient
// Dynamic prop: Solid | BlockLight | TouchAll | ServerEntityOnClient
// Breakable prop: Solid | BlockLight | Debris | PhysicsProp | ServerEntityOnClient
// Transient physics prop: Solid | BlockLight | TouchAll | PhysicsProp | PushAway
// Ladder: Ladder | PassBullets | StaticLevel
// Extendable ladder: PlayerClip | NpcClip | Ladder | StaticLevel
// Breakable prop debris: Solid | BlockLight | Debris | PhysicsProp | PushAway
// Dropped weapon: Solid | BlockLos | BlockLight | Pickup | TouchAll | PhysicsProp | ServerEntityOnClient
// Dropped hostage: Solid | TouchAll | ServerEntityOnClient | CsgoDroppedHostage
// Player entity: TouchAll | Player | ServerEntityOnClient
// Sky box: Sky | StaticLevel
