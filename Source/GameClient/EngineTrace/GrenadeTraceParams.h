#pragma once

#include <CS2/Constants/CollisionGroup.h>
#include <CS2/Constants/InteractionLayers.h>
#include <CS2/Constants/PhysicsQueryFlag.h>

namespace engine_trace::grenade {
    constexpr cs2::CollisionGroup kCollisionGroup{cs2::CollisionGroup::Projectile};

    constexpr auto kQueryFlags =
        cs2::PhysicsQueryFlag::IncludeSolidContacts
        | cs2::PhysicsQueryFlag::RespectDisabledSolidContacts
        | cs2::PhysicsQueryFlag::IncludeTriggerContacts
        | cs2::PhysicsQueryFlag::RespectIgnoredPairs;

    constexpr auto kFirstInteraction =
        cs2::engine_trace::InteractionLayer::CsgoGrenadeClip
        | cs2::engine_trace::InteractionLayer::Solid
        | cs2::engine_trace::InteractionLayer::Window
        | cs2::engine_trace::InteractionLayer::PassBullets;

    constexpr auto kFilterInteractionMask =
        cs2::engine_trace::InteractionLayer::Pickup
        | cs2::engine_trace::InteractionLayer::Player;

    constexpr auto kFilterObjectMask =
        cs2::engine_trace::InteractionLayer::CsgoThrownGrenade
        | cs2::engine_trace::InteractionLayer::Solid
        | cs2::engine_trace::InteractionLayer::TouchAll;
}
