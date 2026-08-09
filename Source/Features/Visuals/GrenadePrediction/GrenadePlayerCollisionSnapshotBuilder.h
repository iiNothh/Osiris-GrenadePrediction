#pragma once

#include <CS2/Classes/ConVarTypes.h>
#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Constants/EntityHandle.h>
#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionMirror.h>
#include <GameClient/Entities/BaseEntity.h>
#include <GameClient/Entities/BaseModelEntity.h>
#include <GameClient/EntitySystem/EntitySystem.h>

template <typename HookContext>
class GrenadePlayerCollisionSnapshotBuilder {
public:
    explicit GrenadePlayerCollisionSnapshotBuilder(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void build(GrenadePlayerCollisionSnapshot& snapshot, cs2::C_BaseEntity* localPawn) const noexcept
    {
        snapshot.reset();
        if (!localPawn || !localPawn->identity || !localPawn->identity->entity || !localPawn->identity->entityClass
            || localPawn->identity->handle.value == cs2::INVALID_EHANDLE_INDEX)
            return;

        const auto teammatesAreEnemies = hookContext.cvarSystem().template getConVarValue<cs2::mp_teammates_are_enemies>();
        const auto localTeam = hookContext.template make<BaseEntity>(localPawn).optionalTeamNumber();
        if (!teammatesAreEnemies.has_value() || !localTeam.hasValue())
            return;

        auto entitySystem = hookContext.template make<EntitySystem>();
        bool failed{};
        entitySystem.forEachNetworkableEntityIdentity([&](const auto& identity) noexcept {
            if (failed || identity.entity == localPawn)
                return;

            if (!identity.entity || !identity.entityClass || identity.handle.value == cs2::INVALID_EHANDLE_INDEX) {
                failed = true;
                return;
            }

            if (!hookContext.entityClassifier().template entityIs<cs2::C_CSPlayerPawn>(identity.entityClass))
                return;

            auto* const player = static_cast<cs2::C_BaseEntity*>(identity.entity);
            auto entity = hookContext.template make<BaseEntity>(player);
            const auto candidateTeam = entity.optionalTeamNumber();
            const auto origin = entity.absOrigin();
            const auto collision = hookContext.template make<BaseModelEntity>(static_cast<cs2::C_BaseModelEntity*>(player)).collisionProperty();
            const auto mins = collision.mins();
            const auto maxs = collision.maxs();
            if (!candidateTeam.hasValue() || !origin.hasValue() || !grenade_player_collision_mirror::finite(origin.value())
                || !mins.hasValue() || !maxs.hasValue()) {
                failed = true;
                return;
            }

            const GrenadePlayerCollisionCandidate candidate{identity.handle.value, origin.value() + mins.value(), origin.value() + maxs.value(),
                *teammatesAreEnemies || localTeam.value() != candidateTeam.value()};
            if (!grenade_player_collision_mirror::validBounds(candidate) || !snapshot.append(candidate))
                failed = true;
        });

        if (failed)
            snapshot.reset();
        else
            snapshot.available = true;
    }

private:
    HookContext& hookContext;
};
