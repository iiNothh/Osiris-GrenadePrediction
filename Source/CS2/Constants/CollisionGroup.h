#pragma once

#include <cstdint>

namespace cs2
{
    enum class CollisionGroup : std::uint8_t {
        Always,
        Never,
        Trigger,
        ConditionallySolid,
        Default,
        Debris,
        InteractiveDebris,
        Interactive,
        Player,
        BreakableGlass,
        Vehicle,
        PlayerMovement,
        Npc,
        InVehicle,
        Weapon,
        Unknown, //Unnamed, unknown
        Projectile,
        DoorBlocker,
        PassableDoor,
        Dissolving,
        PushAway,
        NpcActor,
        NpcScripted,
        PzClip,
        Props
    };
}
