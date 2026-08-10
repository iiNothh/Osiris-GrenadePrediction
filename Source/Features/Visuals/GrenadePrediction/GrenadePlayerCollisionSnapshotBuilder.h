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

    void begin(GrenadePlayerCollisionCollectionScratch& scratch) const noexcept
    {
        scratch.reset();
    }

    void observe(GrenadePlayerCollisionCollectionScratch& scratch, const cs2::CEntityIdentity& identity) const noexcept
    {
        if (!identity.entityClass) {
            ++scratch.malformedUnrelatedIdentityCount;
            return;
        }

        const bool isPlayer = hookContext.entityClassifier().template entityIs<cs2::C_CSPlayerPawn>(identity.entityClass);
        if (!isPlayer) {
            if (!identity.entity || identity.handle.value == cs2::INVALID_EHANDLE_INDEX)
                ++scratch.malformedUnrelatedIdentityCount;
            return;
        }

        if (!identity.entity || identity.handle.value == cs2::INVALID_EHANDLE_INDEX) {
            scratch.playerDataInvalid = true;
            return;
        }

        auto* const player = static_cast<cs2::C_BaseEntity*>(identity.entity);
        auto entity = hookContext.template make<BaseEntity>(player);
        const auto team = entity.optionalTeamNumber();
        const auto origin = entity.absOrigin();
        const auto collision = hookContext.template make<BaseModelEntity>(static_cast<cs2::C_BaseModelEntity*>(player)).collisionProperty();
        const auto mins = collision.mins();
        const auto maxs = collision.maxs();
        if (!team.hasValue() || !origin.hasValue() || !grenade_player_collision_mirror::finite(origin.value()) || !mins.hasValue() || !maxs.hasValue()) {
            scratch.playerDataInvalid = true;
            return;
        }

        const GrenadePlayerCollisionCandidate bounds{identity.handle.value, origin.value() + mins.value(), origin.value() + maxs.value(), false};
        if (!grenade_player_collision_mirror::validBounds(bounds)) {
            scratch.playerDataInvalid = true;
            return;
        }

        if (scratch.count == GrenadePlayerCollisionCollectionScratch::kCapacity) {
            scratch.overflowed = true;
            return;
        }
        scratch.candidates[scratch.count++] = {identity.handle.value, bounds.mins, bounds.maxs, team.value()};
    }

    void finish(GrenadePlayerCollisionSnapshot& snapshot, GrenadePlayerCollisionCollectionScratch& scratch, cs2::C_BaseEntity* localPawn) const noexcept
    {
        if (scratch.playerDataInvalid || scratch.overflowed || !localPawn || !localPawn->identity || localPawn->identity->entity != localPawn
            || !localPawn->identity->entityClass || localPawn->identity->handle.value == cs2::INVALID_EHANDLE_INDEX) {
            commitUnavailable(snapshot, scratch.malformedUnrelatedIdentityCount);
            return;
        }

        if (!hookContext.entityClassifier().template entityIs<cs2::C_CSPlayerPawn>(localPawn->identity->entityClass)) {
            commitUnavailable(snapshot, scratch.malformedUnrelatedIdentityCount);
            return;
        }

        const auto teammatesAreEnemies = hookContext.cvarSystem().template getConVarValue<cs2::mp_teammates_are_enemies>();
        const auto localTeam = hookContext.template make<BaseEntity>(localPawn).optionalTeamNumber();
        if (!teammatesAreEnemies.has_value() || !localTeam.hasValue()) {
            commitUnavailable(snapshot, scratch.malformedUnrelatedIdentityCount);
            return;
        }

        normalizeByRawHandle(scratch);
        const auto localHandle = localPawn->identity->handle.value;
        const bool teammatesEligible = *teammatesAreEnemies;
        const auto localTeamNumber = localTeam.value();
        if (!matchesAvailableSnapshot(snapshot, scratch, localHandle, teammatesEligible, localTeamNumber)) {
            snapshot.count = 0;
            for (int i = 0; i < scratch.count; ++i) {
                const auto& collected = scratch.candidates[i];
                if (collected.rawHandle == localHandle)
                    continue;
                snapshot.candidates[snapshot.count++] = {collected.rawHandle, collected.mins, collected.maxs, teammatesEligible || localTeamNumber != collected.team};
            }
            snapshot.status = GrenadePlayerCollisionSnapshotStatus::Available;
            ++snapshot.revision;
        }
        snapshot.malformedUnrelatedIdentityCount = scratch.malformedUnrelatedIdentityCount;
    }

    void build(GrenadePlayerCollisionSnapshot& snapshot, cs2::C_BaseEntity* localPawn) const noexcept
    {
        begin(snapshot.collectionScratch);
        auto entitySystem = hookContext.template make<EntitySystem>();
        entitySystem.forEachNetworkableEntityIdentity([&](const auto& identity) noexcept {
            if (identity.entity == localPawn)
                return;
            observe(snapshot.collectionScratch, identity);
        });
        finish(snapshot, snapshot.collectionScratch, localPawn);
    }

private:
    static void commitUnavailable(GrenadePlayerCollisionSnapshot& snapshot, std::uint32_t malformedUnrelatedIdentityCount) noexcept
    {
        if (snapshot.status != GrenadePlayerCollisionSnapshotStatus::Unavailable || snapshot.count != 0) {
            snapshot.count = 0;
            snapshot.status = GrenadePlayerCollisionSnapshotStatus::Unavailable;
            ++snapshot.revision;
        }
        snapshot.malformedUnrelatedIdentityCount = malformedUnrelatedIdentityCount;
    }

    [[nodiscard]] static bool candidatesEqual(const GrenadePlayerCollisionCandidate& candidate, const GrenadePlayerCollisionCollectedCandidate& collected,
        bool relationshipEligible) noexcept
    {
        return candidate.rawHandle == collected.rawHandle
            && candidate.mins.x == collected.mins.x && candidate.mins.y == collected.mins.y && candidate.mins.z == collected.mins.z
            && candidate.maxs.x == collected.maxs.x && candidate.maxs.y == collected.maxs.y && candidate.maxs.z == collected.maxs.z
            && candidate.relationshipEligible == relationshipEligible;
    }

    [[nodiscard]] static bool matchesAvailableSnapshot(const GrenadePlayerCollisionSnapshot& snapshot,
        const GrenadePlayerCollisionCollectionScratch& scratch, std::uint32_t localHandle, bool teammatesEligible, TeamNumber localTeam) noexcept
    {
        if (snapshot.status != GrenadePlayerCollisionSnapshotStatus::Available || snapshot.count < 0 || snapshot.count > GrenadePlayerCollisionSnapshot::kCapacity)
            return false;

        int snapshotIndex{};
        for (int i = 0; i < scratch.count; ++i) {
            const auto& collected = scratch.candidates[i];
            if (collected.rawHandle == localHandle)
                continue;
            if (snapshotIndex == snapshot.count || !candidatesEqual(snapshot.candidates[snapshotIndex], collected, teammatesEligible || localTeam != collected.team))
                return false;
            ++snapshotIndex;
        }
        return snapshotIndex == snapshot.count;
    }

    static void normalizeByRawHandle(GrenadePlayerCollisionCollectionScratch& scratch) noexcept
    {
        for (int i = 1; i < scratch.count; ++i) {
            const auto candidate = scratch.candidates[i];
            int insertionIndex = i;
            while (insertionIndex > 0 && scratch.candidates[insertionIndex - 1].rawHandle > candidate.rawHandle) {
                scratch.candidates[insertionIndex] = scratch.candidates[insertionIndex - 1];
                --insertionIndex;
            }
            scratch.candidates[insertionIndex] = candidate;
        }

        int uniqueCount{};
        for (int i = 0; i < scratch.count; ++i) {
            if (uniqueCount && scratch.candidates[uniqueCount - 1].rawHandle == scratch.candidates[i].rawHandle)
                continue;
            scratch.candidates[uniqueCount++] = scratch.candidates[i];
        }
        scratch.count = uniqueCount;
    }

    HookContext& hookContext;
};
