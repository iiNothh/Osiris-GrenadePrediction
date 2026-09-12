#pragma once

#include <CS2/Classes/Entities/C_BaseCSGrenadeProjectile.h>
#include <CS2/Classes/Entities/C_BaseEntity.h>
#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Classes/Entities/GrenadeProjectiles.h>
#include <CS2/Classes/EntitySystem/CEntityIdentity.h>
#include <Features/Visuals/GrenadePrediction/GrenadeGravity.h>
#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionSnapshotBuilder.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/GrenadeSimulator.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeLifecycle.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheUpdater.h>
#include <Features/Visuals/GrenadePrediction/Trajectory.h>
#include <GameClient/Entities/DecoyProjectile.h>
#include <GameClient/Entities/GrenadeKind.h>
#include <GameClient/Entities/GrenadeKindMapper.h>
#include <GameClient/Entities/GrenadeProjectile.h>
#include <GameClient/Entities/SmokeGrenadeProjectile.h>
#include <Platform/GrenadePredictionCapabilities.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

template <typename HookContext>
class LiveGrenadePrediction {
public:
    LiveGrenadePrediction(HookContext& hookContext, GrenadePredictionState& state) noexcept
        : hookContext{hookContext}
        , state{state}
    {
    }

    void beginScan() noexcept
    {
        auto& scratch = hookContext.grenadePredictionPerHookState().playerCollisionCollectionScratch;
        GrenadePlayerCollisionSnapshotBuilder<HookContext>{hookContext}.begin(scratch);
        state.liveGrenadeCache.beginScan();
    }

    void observeEntity(const cs2::CEntityIdentity& identity, EntityTypeInfo type) noexcept
    {
        GrenadePlayerCollisionSnapshotBuilder<HookContext>{hookContext}.observe(hookContext.grenadePredictionPerHookState().playerCollisionCollectionScratch, identity);
        observeLiveProjectile(identity, type);
    }

    void endScan(cs2::C_CSPlayerPawn* localPawn) noexcept
    {
        state.liveGrenadeCache.endScan();
        GrenadePlayerCollisionSnapshotBuilder<HookContext>{hookContext}.finish(state.playerCollisionSnapshot,
            hookContext.grenadePredictionPerHookState().playerCollisionCollectionScratch, static_cast<cs2::C_BaseEntity*>(localPawn));
    }

    template <typename Simulate>
    [[nodiscard]] bool attemptNewestProjectileAdoption(Trajectory& liveGrenadeTrajectoryScratch, cs2::CEntityHandle localPawnHandle,
        Optional<float> currentTime, Simulate&& simulate) noexcept
    {
        if (currentTime.hasValue() && !Math::isFinite(currentTime.value()))
            currentTime = {};
        state.liveGrenadeAuthority.observeLocalPawn(localPawnHandle);
        state.liveGrenadeAuthority.update(state.liveGrenadeCache);
        if (!state.liveGrenadeCache.hasAuthoritativeScan())
            return false;

        const auto projectile = state.liveGrenadeAuthority.newestLocalProjectile(state.liveGrenadeCache);
        if (!projectile.hasValue() || !state.liveGrenadeAuthority.observeForSimulation(projectile.value()))
            return false;
        if (!state.liveGrenadeAuthority.isSimulationRetryDue(projectile.value(), state.frame))
            return false;
        if (!simulate(projectile.value())) {
            state.liveGrenadeAuthority.recordSimulationFailure(projectile.value(), state.frame);
            return false;
        }

        state.commitLiveGrenadeTrajectory(liveGrenadeTrajectoryScratch);
        state.finalizeStagedTrajectory(true, currentTime.hasValue(), currentTime.valueOr(0.0f));
        state.liveGrenadeAuthority.accept(projectile.value(), currentTime);
        state.invalidateTempTrajectory();
        return true;
    }

    [[nodiscard]] bool attemptNewestProjectileAdoption(Trajectory& liveGrenadeTrajectoryScratch, cs2::CEntityHandle localPawnHandle,
        Optional<float> currentTime, cs2::C_CSPlayerPawn* pawn) noexcept
    {
        return attemptNewestProjectileAdoption(liveGrenadeTrajectoryScratch, localPawnHandle, currentTime, [&](const auto& projectile) noexcept {
            return simulateProjectileTrajectory(liveGrenadeTrajectoryScratch, projectile, pawn);
        });
    }

private:
    void observeLiveProjectile(const cs2::CEntityIdentity& identity, EntityTypeInfo type) noexcept
    {
        if constexpr (!GrenadePredictionPlatformCapabilities::supportsLiveProjectilePrediction)
            return;
        else {
            const auto kind = GrenadeKindMapper::fromProjectile(type);
            if (kind == GrenadeKind::None || !identity.entity)
                return;

            const auto grenade = GrenadeProjectile{hookContext, static_cast<cs2::C_BaseCSGrenadeProjectile*>(identity.entity)};
            LiveGrenadeLifecycleState lifecycleState;
            if (kind == GrenadeKind::SmokeGrenade) {
                lifecycleState.smokeEffectStarted = SmokeGrenadeProjectile{hookContext, static_cast<cs2::C_SmokeGrenadeProjectile*>(identity.entity)}.didSmokeEffect();
                if (!lifecycleState.smokeEffectStarted.hasValue() || lifecycleState.smokeEffectStarted.value()) {
                    state.liveGrenadeCache.invalidate(identity.handle);
                    return;
                }
            }
            else if (kind == GrenadeKind::HEGrenade) {
                static_cast<void>(updateHELiveGrenade(grenade, identity.handle));
                return;
            }
            else if (kind == GrenadeKind::Decoy) {
                static_cast<void>(updateDecoyLiveGrenade(grenade, identity.handle,
                    DecoyProjectile{hookContext, static_cast<cs2::C_DecoyProjectile*>(identity.entity)}));
                return;
            }
            static_cast<void>(LiveGrenadeCacheUpdater{state.liveGrenadeCache}.update(grenade, identity.handle, kind, lifecycleState));
        }
    }

    [[nodiscard]] bool simulateProjectileTrajectory(Trajectory& liveGrenadeTrajectoryScratch, const LiveGrenadeSnapshot& projectile,
        cs2::C_CSPlayerPawn* pawn) noexcept
    {
        auto simulator = hookContext.template make<GrenadeSimulator>();
        liveGrenadeTrajectoryScratch.clear();
        simulator.setPlayerCollisionSnapshot(&state.playerCollisionSnapshot);
        const auto serverGravity = grenade_prediction::resolveServerGravity(hookContext.cvarSystem());
        simulator.simulate(liveGrenadeTrajectoryScratch, {projectile.initialPosition, projectile.initialVelocity}, projectile.kind, pawn, serverGravity);
        return liveGrenadeTrajectoryScratch.valid && liveGrenadeTrajectoryScratch.pointsCount;
    }

    template <typename Projectile>
    [[nodiscard]] bool updateHELiveGrenade(const Projectile& projectile, cs2::CEntityHandle projectileHandle) noexcept
    {
        return LiveGrenadeCacheUpdater{state.liveGrenadeCache}.update(projectile, projectileHandle, GrenadeKind::HEGrenade,
            {.heExplodeEffectTickBegin = projectile.explodeEffectTickBegin()});
    }

    template <typename Projectile, typename Decoy>
    [[nodiscard]] bool updateDecoyLiveGrenade(const Projectile& projectile, cs2::CEntityHandle projectileHandle, const Decoy& decoy) noexcept
    {
        return LiveGrenadeCacheUpdater{state.liveGrenadeCache}.update(projectile, projectileHandle, GrenadeKind::Decoy, {.decoyShotTick = decoy.decoyShotTick()});
    }

    HookContext& hookContext;
    GrenadePredictionState& state;
};
