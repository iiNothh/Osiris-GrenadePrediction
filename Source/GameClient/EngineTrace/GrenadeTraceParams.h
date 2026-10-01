#pragma once

#include <CS2/Classes/Vector.h>
#include <CS2/Constants/CollisionGroup.h>
#include <CS2/Constants/InteractionLayers.h>
#include <CS2/Constants/PhysicsQueryFlag.h>
#include <GameClient/EngineTrace/TraceFilter.h>

namespace engine_trace::grenade {
    using InteractionLayer = cs2::engine_trace::InteractionLayer;

    constexpr cs2::Vector kHullMins{-2.0f, -2.0f, -2.0f};
    constexpr cs2::Vector kHullMaxs{2.0f, 2.0f, 2.0f};
    constexpr cs2::CollisionGroup kCollisionGroup{cs2::CollisionGroup::Projectile};

    constexpr auto kSpawnInteractionLayers =
        InteractionLayer::Solid
        | InteractionLayer::Hitboxes
        | InteractionLayer::Sky
        | InteractionLayer::PassBullets
        | InteractionLayer::Player
        | InteractionLayer::Npc
        | InteractionLayer::Debris;

    constexpr auto kGrenadeInteractionLayers =
        InteractionLayer::CsgoGrenadeClip
        | InteractionLayer::Solid
        | InteractionLayer::Window
        | InteractionLayer::PassBullets;

    constexpr auto kSpawnQueryFlags =
        cs2::PhysicsQueryFlag::IncludeSolidContacts
        | cs2::PhysicsQueryFlag::RespectDisabledSolidContacts
        | cs2::PhysicsQueryFlag::IncludeTriggerContacts;

    constexpr auto kQueryFlags = kSpawnQueryFlags
        | cs2::PhysicsQueryFlag::RespectIgnoredPairs;

    constexpr auto kFilterInteractionMask =
        InteractionLayer::Pickup
        | InteractionLayer::Player;

    constexpr auto kFilterObjectMask =
        InteractionLayer::CsgoThrownGrenade
        | InteractionLayer::Solid
        | InteractionLayer::TouchAll;

    constexpr TraceFilterParameters kSpawnFilter{
        .interactsWith = kSpawnInteractionLayers,
        .collisionGroup = cs2::CollisionGroup::Default,
        .queryFlags = kSpawnQueryFlags
    };

    constexpr TraceFilterParameters kInFlightFilter{
        .interactsWith = kGrenadeInteractionLayers,
        .collisionGroup = kCollisionGroup,
        .queryFlags = kQueryFlags
    };

    constexpr TraceFilterParameters kLaunchEndpointFilter{
        .interactsWith = kGrenadeInteractionLayers,
        .collisionGroup = cs2::CollisionGroup::Default,
        .queryFlags = kQueryFlags
    };

}
