#pragma once

#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Classes/Entities/WeaponEntities.h>
#include <Features/Visuals/GrenadePrediction/GrenadeGravity.h>
#include <GameClient/Entities/GrenadeKind.h>
#include <GameClient/Entities/GrenadeKindMapper.h>
#include <GameClient/Entities/GrenadeWeapon.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeLaunchSelection.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeLaunchFallback.h>
#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionSnapshotBuilder.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionConfigVariables.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionController.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Rendering/GrenadeTrajectoryRenderer.h>
#include <Features/Visuals/GrenadePrediction/GrenadeSimulator.h>
#include <Features/Visuals/GrenadePrediction/GrenadeTracePreset.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheUpdater.h>
#include <GameClient/Entities/GrenadeProjectile.h>
#include <GameClient/Entities/DecoyProjectile.h>
#include <GameClient/Entities/PlayerPawn.h>
#include <GameClient/Entities/SmokeGrenadeProjectile.h>
#include <GameClient/EngineTrace/EngineTrace.h>
#include <GameClient/GlobalVars.h>
#include <GameClient/GrenadePrediction/GrenadeLaunch.h>
#include <GameClient/Panorama/PanoramaUiEngine.h>
#include <HookContext/HookContextMacros.h>
#include <Platform/GrenadePredictionCapabilities.h>
#include <Utils/Math.h>

template <typename HookContext>
class GrenadePrediction {
public:
    explicit GrenadePrediction(HookContext& hookContext) noexcept : hookContext{hookContext} {}

    void beginLiveGrenadeScan() noexcept
    {
        auto& state = this->state();
        GrenadePlayerCollisionSnapshotBuilder<HookContext>{hookContext}.begin(hookContext.grenadePredictionPerHookState().playerCollisionCollectionScratch);
        LiveGrenadeCacheUpdater{state.liveGrenadeCache}.beginScan();
    }

    void endLiveGrenadeScan(cs2::C_CSPlayerPawn* localPawn) noexcept
    {
        auto& state = this->state();
        LiveGrenadeCacheUpdater{state.liveGrenadeCache}.endScan();
        GrenadePlayerCollisionSnapshotBuilder<HookContext>{hookContext}.finish(state.playerCollisionSnapshot, hookContext.grenadePredictionPerHookState().playerCollisionCollectionScratch,
            static_cast<cs2::C_BaseEntity*>(localPawn));
    }

    void updateLiveGrenade(const cs2::CEntityIdentity& identity, EntityTypeInfo type) noexcept
    {
        GrenadePlayerCollisionSnapshotBuilder<HookContext>{hookContext}.observe(hookContext.grenadePredictionPerHookState().playerCollisionCollectionScratch, identity);
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
                    state().liveGrenadeCache.invalidate(identity.handle);
                    return;
                }
            }
            else if (kind == GrenadeKind::HEGrenade)
                return static_cast<void>(GrenadePredictionController::updateHELiveGrenade(state().liveGrenadeCache, grenade, identity.handle));
            else if (kind == GrenadeKind::Decoy)
                return static_cast<void>(GrenadePredictionController::updateDecoyLiveGrenade(state().liveGrenadeCache, grenade, identity.handle,
                    DecoyProjectile{hookContext, static_cast<cs2::C_DecoyProjectile*>(identity.entity)}));
            static_cast<void>(LiveGrenadeCacheUpdater{state().liveGrenadeCache}.update(grenade, identity.handle, kind, lifecycleState));
        }
    }

    void handleGrenadePrediction(auto&& playerPawn, auto&& activeWeapon, cs2::CEntityHandle localPawnHandle, bool enabled) noexcept
    {
        if (!playerPawn.isControlledByLocalPlayer())
            return;
        auto& state = this->state();
        GrenadePredictionController::beginFrame(state);
        const auto rawCurtime = hookContext.globalVars().curtime();
        const Optional<float> curtime = rawCurtime.hasValue() && Math::isFinite(rawCurtime.value()) ? rawCurtime : Optional<float>{};
        const bool hasCurtime = curtime.hasValue();
        const float time = curtime.valueOr(0.0f);
        auto* const pawn = static_cast<cs2::C_CSPlayerPawn*>(static_cast<cs2::C_BaseEntity*>(playerPawn.baseEntity()));
        const auto presentCachedTrajectory = [&]() noexcept { applyCachedTrajectoryPresentation(hasCurtime, time); };
        if constexpr (!GrenadePredictionPlatformCapabilities::supportsHeldPrediction) {
            clearPrediction();
            presentCachedTrajectory();
            return;
        } else {
            if (!grenade_trace_preset::isInFlightTraceAvailable(hookContext.template make<EngineTrace>())) {
                clearPrediction();
                return;
            }
            if (!playerPawn.isAlive().value_or(false) || !enabled) {
                clearPrediction();
                presentCachedTrajectory();
                return;
            }
            if constexpr (GrenadePredictionPlatformCapabilities::supportsLiveProjectilePrediction) {
                auto& liveGrenadeTrajectoryScratch = hookContext.grenadePredictionPerHookState().liveGrenadeTrajectoryScratch;
                if (GrenadePredictionController::acceptNewestLiveGrenade(state, liveGrenadeTrajectoryScratch, localPawnHandle, curtime, [&](const auto& projectile) noexcept {
                    auto simulator = hookContext.template make<GrenadeSimulator>();
                    liveGrenadeTrajectoryScratch.clear();
                    simulator.setPlayerCollisionSnapshot(&state.playerCollisionSnapshot);
                    simulator.simulate(liveGrenadeTrajectoryScratch, {projectile.initialPosition, projectile.initialVelocity}, projectile.kind, pawn,
                        grenade_prediction::resolveServerGravity(hookContext.cvarSystem()));
                    return liveGrenadeTrajectoryScratch.valid && liveGrenadeTrajectoryScratch.pointsCount;
                }))
                    hideLivePrediction();
            }
            static_cast<void>(GrenadePredictionController::observeCurrentTime(state, curtime));
            const auto rawFrametime = hookContext.globalVars().frametime();
            const Optional<float> frametime = rawFrametime.hasValue() && Math::isFinite(rawFrametime.value()) ? rawFrametime : Optional<float>{};
            bool shouldUpdate = GrenadePredictionController::advanceScheduler(state.updateScheduler, false, frametime);
            const auto pendingWeapon = state.throwObservation.pendingWeapon();
            if (GrenadePredictionController::completeHeldThrow(state, pendingWeapon, hasCurtime, time)) {
                hideLivePrediction();
                presentCachedTrajectory();
                return;
            }
            const auto kind = GrenadeKindMapper::from(activeWeapon.baseEntity().classify());
            auto* const weapon = static_cast<cs2::C_BaseCSGrenade*>(static_cast<cs2::C_BaseEntity*>(activeWeapon.baseEntity()));
            const auto weaponHandle = activeWeapon.baseEntity().handle();
            if (!weapon || kind == GrenadeKind::None || weaponHandle == cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}) {
                state.throwObservation.reset();
                state.invalidateTempTrajectory();
                hideLivePrediction();
                presentCachedTrajectory();
                return;
            }

            const auto rawThrowTime = hookContext.template make<GrenadeWeapon>(weapon).throwTime();
            const Optional<float> throwTime = rawThrowTime.hasValue() && Math::isFinite(rawThrowTime.value()) ? rawThrowTime : Optional<float>{};
            const bool releaseEdge = observeHeldThrow(weapon, weaponHandle, throwTime);
            const bool scheduledTransition = throwTime.hasValue() && state.throwObservation.observeThrowTime(weaponHandle, throwTime.value());
            if (GrenadePredictionController::completeHeldThrow(state, state.throwObservation.pendingWeapon(), hasCurtime, time)
                || (!throwTime.hasValue() && GrenadePredictionController::completeLegacyHeldThrow(state, weaponHandle, releaseEdge, hasCurtime, time))) {
                hideLivePrediction();
                presentCachedTrajectory();
                return;
            }
            if (scheduledTransition)
                shouldUpdate = GrenadePredictionController::advanceScheduler(state.updateScheduler, true, frametime);
            if (!shouldUpdate) {
                if (!state.liveGrenadeAuthority.blocksHeldPrediction() && state.ownsTempTrajectory(weaponHandle, state.throwObservation.sequence))
                    drawTrajectory(state.tempTrajectory, state.liveContainerPanelHandle, state.livePresentationState);
                else if (state.liveGrenadeAuthority.blocksHeldPrediction())
                    hideLivePrediction();
                presentCachedTrajectory();
                return;
            }
            auto simulator = hookContext.template make<GrenadeSimulator>();
            const auto launch = prepareGrenadeLaunch(state.throwObservation.isFinalized(), state.throwObservation.hasRetainedThrowStrength,
            [&]() noexcept { return hookContext.template make<GrenadeLaunch<HookContext>>().get(weapon, pawn); },
            [&]() noexcept { return computeHeldGrenadeLaunchFallback(playerPawn, simulator, state.throwObservation.retainedThrowStrength, pawn); });
            if (!launch.hasValue()) {
                hideLivePrediction(state.throwObservation.hasPendingExecution());
                if (!state.throwObservation.hasPendingExecution())
                    state.invalidateTempTrajectory();
                presentCachedTrajectory();
                return;
            }

            const auto gravity = grenade_prediction::resolveServerGravity(hookContext.cvarSystem());
            const HeldGrenadeSimulationInput input{launch.value().origin, launch.value().velocity, kind, gravity, state.playerCollisionSnapshot.revision,
                localPawnHandle, weaponHandle, state.throwObservation.sequence};
            if (GrenadePredictionController::isHeldSimulationNecessary(state, input, shouldUpdate)) {
                simulator.setPlayerCollisionSnapshot(&state.playerCollisionSnapshot);
                simulator.simulate(state.tempTrajectory, launch.value(), kind, pawn, gravity);
                const bool succeeded = state.tempTrajectory.valid && state.tempTrajectory.pointsCount;
                GrenadePredictionController::recordHeldSimulationResult(state, input, succeeded);
                if (!succeeded)
                    hideLivePrediction(state.throwObservation.hasPendingExecution());
            }
            if (!state.liveGrenadeAuthority.blocksHeldPrediction() && state.ownsTempTrajectory(weaponHandle, state.throwObservation.sequence))
                drawTrajectory(state.tempTrajectory, state.liveContainerPanelHandle, state.livePresentationState);
            else if (state.liveGrenadeAuthority.blocksHeldPrediction())
                hideLivePrediction();
            presentCachedTrajectory();
        }
    }

    void handleNoLocalPawn() noexcept
    {
        auto& state = this->state();
        GrenadePredictionController::beginFrame(state);
        if constexpr (GrenadePredictionPlatformCapabilities::supportsHeldPrediction) {
            if (!grenade_trace_preset::isInFlightTraceAvailable(hookContext.template make<EngineTrace>())) {
                clearPrediction();
                return;
            }
        }
        const auto rawCurtime = hookContext.globalVars().curtime();
        const Optional<float> curtime = rawCurtime.hasValue() && Math::isFinite(rawCurtime.value()) ? rawCurtime : Optional<float>{};
        static_cast<void>(GrenadePredictionController::observeCurrentTime(state, curtime));
        clearPrediction();
        applyCachedTrajectoryPresentation(curtime.hasValue(), curtime.valueOr(0.0f));
    }

    void clearPrediction() noexcept
    {
        auto& state = this->state();
        GrenadePredictionController::clearPredictionAndHidePanels(state,
            [this] { hideLivePrediction(); }, [this, &state] { renderer().hide(state.lastCacheContainerPanelHandle); });
    }

    void onUnload() noexcept
    {
        clearPrediction();
        auto& state = this->state();
        state.liveGrenadeCache.clear();
        auto&& uiEngine = hookContext.template make<PanoramaUiEngine>();
        uiEngine.deletePanelByHandle(state.liveContainerPanelHandle);
        uiEngine.deletePanelByHandle(state.lastCacheContainerPanelHandle);
        state.liveContainerPanelHandle = {};
        state.lastCacheContainerPanelHandle = {};
        GrenadePredictionController::resetPresentationState(state);
    }

private:
    [[nodiscard]] auto& state() const noexcept { return hookContext.featuresStates().visualFeaturesStates.grenadePredictionState; }
    [[nodiscard]] auto renderer() noexcept { return hookContext.template make<GrenadeTrajectoryRenderer>(); }
    void hideLivePrediction(bool preserve = false) noexcept
    {
        auto& state = this->state();
        renderer().hide(state.liveContainerPanelHandle);
        if (!preserve)
            state.invalidateTempTrajectory();
    }
    [[nodiscard]] bool observeHeldThrow(cs2::C_BaseCSGrenade* weapon, cs2::CEntityHandle weaponHandle, Optional<float> throwTime) noexcept
    {
        auto& observation = state().throwObservation;
        if (observation.observeWeapon(weaponHandle))
            state().invalidateTempTrajectory();
        bool pinPulled{};
        bool releaseEdge{};
        const auto grenadeWeapon = hookContext.template make<GrenadeWeapon>(weapon);
        if (const auto pinState = grenadeWeapon.pinPulled(); pinState.hasValue()) {
            pinPulled = pinState.value();
            releaseEdge = GrenadePredictionController::observeHeldThrow(observation, weaponHandle, pinPulled);
        }
        GrenadePredictionController::captureThrowStrength(observation, pinPulled, throwTime, [&]() noexcept {
            return grenadeWeapon.throwStrength();
        });
        return releaseEdge;
    }
    void drawTrajectory(const Trajectory& trajectory, cs2::PanelHandle& panel, GrenadeTrajectoryPresentationState& presentation) noexcept
    {
        const auto hue = static_cast<color::HueInteger>(GET_CONFIG_VAR(grenade_prediction_vars::TrajectoryHue)).toHueFloat();
        const auto bounceHue = static_cast<color::HueInteger>(GET_CONFIG_VAR(grenade_prediction_vars::BounceHue)).toHueFloat();
        renderer().draw(trajectory, panel, presentation, hookContext.hud().getHudReticle(), hue, bounceHue);
    }
    void applyCachedTrajectoryPresentation(bool hasCurtime, float curtime) noexcept
    {
        auto& state = this->state();
        const auto decision = GrenadePredictionController::makeCachedTrajectoryPresentationDecision(state,
            GET_CONFIG_VAR(grenade_prediction_vars::LastTrajectoryVisibility), GET_CONFIG_VAR(grenade_prediction_vars::CacheDuration), hasCurtime, curtime,
            acceptedProjectilePresent(state, hasCurtime, curtime));
        GrenadePredictionController::applyCachedTrajectoryPresentationDecision(state, decision,
            [this, &state] { drawTrajectory(state.lastCommittedTrajectory, state.lastCacheContainerPanelHandle, state.lastCachePresentationState); },
            [this] { hideLivePrediction(); }, [this, &state] { renderer().hide(state.lastCacheContainerPanelHandle); });
    }
    [[nodiscard]] static bool acceptedProjectilePresent(const GrenadePredictionState& state, bool hasCurtime, float curtime) noexcept
    {
        return state.liveGrenadeAuthority.hasAcceptedLiveProjectile() && !state.liveGrenadeAuthority.isFlashbangInEarlyHideWindow(hasCurtime ? Optional<float>{curtime} : Optional<float>{})
            && state.liveGrenadeCache.contains(state.liveGrenadeAuthority.acceptedLiveProjectile());
    }
    HookContext& hookContext;
};
