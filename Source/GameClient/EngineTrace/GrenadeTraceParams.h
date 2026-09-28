#pragma once

#include <CS2/Classes/Vector.h>
#include <CS2/Constants/CollisionGroup.h>
#include <CS2/Constants/InteractionLayers.h>
#include <CS2/Constants/PhysicsQueryFlag.h>
#include <GameClient/EngineTrace/TraceFilter.h>

namespace engine_trace::grenade {
    constexpr cs2::Vector kHullMins{-2.0f, -2.0f, -2.0f};
    constexpr cs2::Vector kHullMaxs{2.0f, 2.0f, 2.0f};
    constexpr cs2::CollisionGroup kCollisionGroup{cs2::CollisionGroup::Projectile};

    constexpr auto kQueryFlags =
        cs2::PhysicsQueryFlag::IncludeSolidContacts
        | cs2::PhysicsQueryFlag::RespectDisabledSolidContacts
        | cs2::PhysicsQueryFlag::IncludeTriggerContacts
        | cs2::PhysicsQueryFlag::RespectIgnoredPairs;

    constexpr TraceFilterParameters kSpawnFilter{
        .interactsWith = cs2::engine_trace::InteractionLayer::Solid | cs2::engine_trace::InteractionLayer::Hitboxes
            | cs2::engine_trace::InteractionLayer::Sky | cs2::engine_trace::InteractionLayer::PassBullets
            | cs2::engine_trace::InteractionLayer::Player | cs2::engine_trace::InteractionLayer::Npc | cs2::engine_trace::InteractionLayer::Debris,
        .collisionGroup = cs2::CollisionGroup::Default,
        .queryFlags = cs2::PhysicsQueryFlag::IncludeSolidContacts
            | cs2::PhysicsQueryFlag::RespectDisabledSolidContacts | cs2::PhysicsQueryFlag::IncludeTriggerContacts
    };

    constexpr TraceFilterParameters kInFlightFilter{
        .interactsWith = cs2::engine_trace::InteractionLayer::CsgoGrenadeClip
            | cs2::engine_trace::InteractionLayer::Solid
            | cs2::engine_trace::InteractionLayer::Window
            | cs2::engine_trace::InteractionLayer::PassBullets,
        .collisionGroup = kCollisionGroup,
        .queryFlags = kQueryFlags
    };

    constexpr TraceFilterParameters kLaunchEndpointFilter{
        .interactsWith = cs2::engine_trace::InteractionLayer::CsgoGrenadeClip
            | cs2::engine_trace::InteractionLayer::Solid
            | cs2::engine_trace::InteractionLayer::Window
            | cs2::engine_trace::InteractionLayer::PassBullets,
        .collisionGroup = cs2::CollisionGroup::Default,
        .queryFlags = cs2::PhysicsQueryFlag::IncludeSolidContacts
            | cs2::PhysicsQueryFlag::RespectDisabledSolidContacts
            | cs2::PhysicsQueryFlag::IncludeTriggerContacts
            | cs2::PhysicsQueryFlag::RespectIgnoredPairs
    };

    constexpr auto kFilterInteractionMask =
        cs2::engine_trace::InteractionLayer::Pickup
        | cs2::engine_trace::InteractionLayer::Player;

    constexpr auto kFilterObjectMask =
        cs2::engine_trace::InteractionLayer::CsgoThrownGrenade
        | cs2::engine_trace::InteractionLayer::Solid
        | cs2::engine_trace::InteractionLayer::TouchAll;

}
