#pragma once

#include <type_traits>

#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Classes/Entities/WeaponEntities.h>
#include <Features/Visuals/GrenadePrediction/GrenadeGravity.h>
#include <Features/Visuals/GrenadePrediction/GrenadeKindMapper.h>
#include <Features/Visuals/GrenadePrediction/GrenadeLaunchSelection.h>
#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionSnapshotBuilder.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionConfigVariables.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionController.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionContext.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/GrenadeSimulator.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheUpdater.h>
#include <GameClient/Entities/GrenadeProjectile.h>
#include <GameClient/Entities/DecoyProjectile.h>
#include <GameClient/Entities/PlayerPawn.h>
#include <GameClient/Entities/SmokeGrenadeProjectile.h>
#include <GameClient/GlobalVars.h>
#include <GameClient/Panorama/PanoramaUiEngine.h>
#include <MemoryPatterns/PatternTypes/EntityPatternTypes.h>
#include <MemoryPatterns/PatternTypes/WeaponPatternTypes.h>

template <typename HookContext>
class GrenadePrediction {
public:
    explicit GrenadePrediction(HookContext& hookContext) noexcept : hookContext{hookContext} {}

    void beginLiveGrenadeScan() noexcept { LiveGrenadeCacheUpdater{context().state().liveGrenadeCache}.beginScan(); }
    void endLiveGrenadeScan() noexcept { LiveGrenadeCacheUpdater{context().state().liveGrenadeCache}.endScan(); }

    void updateLiveGrenade(const cs2::CEntityIdentity& identity, EntityTypeInfo type) noexcept
    {
        const auto kind = GrenadeKindMapper::fromProjectile(type);
        if (kind == cs2::GrenadeKind::None || !identity.entity)
            return;
        const auto grenade = GrenadeProjectile{hookContext, static_cast<cs2::C_BaseCSGrenadeProjectile*>(identity.entity)};
        LiveGrenadeLifecycleState lifecycleState;
        if (kind == cs2::GrenadeKind::SmokeGrenade) {
            lifecycleState.smokeEffectStarted = SmokeGrenadeProjectile{hookContext, static_cast<cs2::C_SmokeGrenadeProjectile*>(identity.entity)}.didSmokeEffect();
            if (!lifecycleState.smokeEffectStarted.hasValue() || lifecycleState.smokeEffectStarted.value()) {
                context().state().liveGrenadeCache.invalidate(identity.handle);
                return;
            }
        }
        else if (kind == cs2::GrenadeKind::HEGrenade)
            return static_cast<void>(GrenadePredictionController::updateHELiveGrenade(context().state().liveGrenadeCache, grenade, identity.handle));
        else if (kind == cs2::GrenadeKind::Decoy)
            return static_cast<void>(GrenadePredictionController::updateDecoyLiveGrenade(context().state().liveGrenadeCache, grenade, identity.handle,
                DecoyProjectile{hookContext, static_cast<cs2::C_DecoyProjectile*>(identity.entity)}));
        static_cast<void>(LiveGrenadeCacheUpdater{context().state().liveGrenadeCache}.update(grenade, identity.handle, kind, lifecycleState));
    }

    void handleGrenadePrediction(auto&& playerPawn, auto&& activeWeapon, cs2::CEntityHandle localPawnHandle, bool enabled) noexcept
    {
        if (!playerPawn.isControlledByLocalPlayer())
            return;
        auto& state = context().state();
        state.liveGrenadeAuthority.observeLocalPawn(localPawnHandle);
        if (!playerPawn.isAlive().value_or(false) || !enabled) { clearPrediction(); return; }
        const auto curtime = hookContext.globalVars().curtime();
        const bool hasCurtime = curtime.hasValue();
        const float time = curtime.valueOr(0.0f);
        auto* const pawn = static_cast<cs2::C_CSPlayerPawn*>(static_cast<cs2::C_BaseEntity*>(playerPawn.baseEntity()));
        if (acceptNewestLiveGrenade(pawn, hasCurtime, time))
            hideLive();
        renderCachedTrajectory(hasCurtime, time);

        const auto pendingWeapon = state.throwObservation.pendingWeapon();
        if (GrenadePredictionController::completeHeldThrow(state, pendingWeapon, hasCurtime, time)) {
            hideLive();
            renderCachedTrajectory(hasCurtime, time);
            return;
        }
        const auto kind = GrenadeKindMapper::from(activeWeapon.baseEntity().classify());
        auto* const weapon = static_cast<cs2::C_BaseCSGrenade*>(static_cast<cs2::C_BaseEntity*>(activeWeapon.baseEntity()));
        if (!weapon || kind == cs2::GrenadeKind::None) { state.throwObservation.reset(); state.invalidateTempTrajectory(); hideLive(); renderCachedTrajectory(hasCurtime, time); return; }

        const auto throwTime = hookContext.patternSearchResults().template get<OffsetToThrowTime>().of(weapon).toOptional();
        const bool releaseEdge = observeHeldThrow(weapon, throwTime);
        const bool scheduledTransition = throwTime.hasValue() && state.throwObservation.observeThrowTime(weapon, throwTime.value());
        if (GrenadePredictionController::completeHeldThrow(state, state.throwObservation.pendingWeapon(), hasCurtime, time)) {
            hideLive();
            renderCachedTrajectory(hasCurtime, time);
            return;
        }
        if (!throwTime.hasValue() && GrenadePredictionController::completeLegacyHeldThrow(state, weapon, releaseEdge, hasCurtime, time)) {
            hideLive();
            renderCachedTrajectory(hasCurtime, time);
            return;
        }
        bool shouldUpdate = state.updateScheduler.shouldUpdate(false, hookContext.globalVars().frametime().hasValue(), hookContext.globalVars().frametime().valueOr(0.0f));
        if (scheduledTransition)
            shouldUpdate = state.updateScheduler.shouldUpdate(true, hookContext.globalVars().frametime().hasValue(), hookContext.globalVars().frametime().valueOr(0.0f));
        if (!shouldUpdate) return;
        auto simulator = hookContext.template make<GrenadeSimulator>();
        const auto launch = prepareGrenadeLaunch(false, shouldUpdate, state.throwObservation.isFinalized(), state.throwObservation.hasRetainedThrowStrength,
            [&]() noexcept { return hookContext.template make<GrenadeLaunch<HookContext>>().get(weapon, pawn); },
            [&]() noexcept -> Optional<GrenadeLaunchState> {
                const auto eyeAngles = playerPawn.eyeAngles();
                const auto origin = playerPawn.absOrigin();
                if (!eyeAngles.hasValue() || !origin.hasValue())
                    return {};
                float eyeHeight = grenade_prediction_params::kDefaultEyeHeight;
                if (const auto viewOffset = hookContext.patternSearchResults().template get<OffsetToViewOffset>().of(static_cast<cs2::C_BaseEntity*>(pawn)).toOptional(); viewOffset.hasValue()
                    && viewOffset.value().z > 30.0f && viewOffset.value().z < 70.0f)
                    eyeHeight = viewOffset.value().z;
                const auto spawn = simulator.computeSpawnPosition(origin.value() + cs2::Vector{0.0f, 0.0f, eyeHeight}, eyeAngles.value(), state.throwObservation.retainedThrowStrength, pawn);
                if (!spawn.hasValue())
                    return {};
                auto velocity = GrenadeSimulator<HookContext>::computeInitialVelocity(eyeAngles.value(), grenade_prediction_params::kBaseThrowVelocity, state.throwObservation.retainedThrowStrength);
                if (const auto playerVelocity = playerPawn.baseEntity().absVelocity(); playerVelocity.hasValue())
                    velocity = velocity + playerVelocity.value() * grenade_prediction_params::kPlayerVelocityScale;
                return GrenadeLaunchState{spawn.value(), velocity};
            });
        if (launch.status != GrenadeLaunchPreparationStatus::Ready) { if (launch.status != GrenadeLaunchPreparationStatus::Unscheduled) hideLive(); renderCachedTrajectory(hasCurtime, time); return; }

        const auto gravity = grenade_prediction::resolveServerGravity(hookContext.cvarSystem());
        GrenadePlayerCollisionSnapshotBuilder<HookContext>{hookContext}.build(state.playerCollisionSnapshot, pawn);
        simulator.setPlayerCollisionSnapshot(&state.playerCollisionSnapshot);
        simulator.simulate(state.tempTrajectory, launch.state.value(), kind, pawn, gravity);
        if (!state.tempTrajectory.valid || !state.tempTrajectory.pointsCount) { state.invalidateTempTrajectory(); hideLive(); renderCachedTrajectory(hasCurtime, time); return; }
        state.tagTempTrajectory(weapon, state.throwObservation.sequence);
        draw(state.tempTrajectory, state.liveContainerPanelHandle, state.livePresentationState);
        renderCachedTrajectory(hasCurtime, time);
    }

    void clearPrediction() noexcept
    {
        auto& state = context().state();
        state.throwObservation.reset();
        state.updateScheduler.reset();
        state.liveGrenadeAuthority.reset();
        state.lastCommitCurtime = 0.0f;
        state.lastValidCurtime = 0.0f;
        state.hasCommitCurtime = false;
        state.hasLastValidCurtime = false;
        state.rollbackDetected = false;
        state.invalidateTempTrajectory();
        state.invalidateCommittedTrajectory();
        state.liveGrenadeTrajectoryScratch.clear();
        renderer().hide(state.liveContainerPanelHandle);
        renderer().hide(state.lastCacheContainerPanelHandle);
    }

    void onUnload() noexcept
    {
        clearPrediction();
        auto& state = context().state();
        state.liveGrenadeCache.clear();
        auto&& uiEngine = hookContext.template make<PanoramaUiEngine>();
        uiEngine.deletePanelByHandle(state.liveContainerPanelHandle);
        uiEngine.deletePanelByHandle(state.lastCacheContainerPanelHandle);
        state.liveContainerPanelHandle = {};
        state.lastCacheContainerPanelHandle = {};
    }

private:
    [[nodiscard]] decltype(auto) context() const noexcept { return hookContext.template make<GrenadePredictionContext>(); }
    [[nodiscard]] auto renderer() noexcept { return hookContext.template make<GrenadeTrajectoryRenderer>(); }
    void hideLive() noexcept { renderer().hide(context().state().liveContainerPanelHandle); }
    [[nodiscard]] bool observeHeldThrow(cs2::C_BaseCSGrenade* weapon, Optional<float> throwTime) noexcept
    {
        auto& observation = context().state().throwObservation;
        if (observation.observeWeapon(weapon))
            context().state().invalidateTempTrajectory();
        bool pinPulled{};
        bool releaseEdge{};
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<OffsetToPinPulled>()) {
            if (const auto pinState = hookContext.patternSearchResults().template get<OffsetToPinPulled>().of(weapon).toOptional(); pinState.hasValue()) {
                pinPulled = pinState.value();
                releaseEdge = GrenadePredictionController::observeHeldThrow(observation, weapon, pinPulled);
            }
        }
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<OffsetToThrowStrength>()) {
            GrenadePredictionController::captureThrowStrength(observation, pinPulled, throwTime, [&]() noexcept {
                return hookContext.patternSearchResults().template get<OffsetToThrowStrength>().of(weapon).toOptional();
            });
        }
        return releaseEdge;
    }
    void draw(const Trajectory& trajectory, cs2::PanelHandle& panel, GrenadeTrajectoryPresentationState& presentation) noexcept
    {
        const auto hue = static_cast<color::HueInteger>(GET_CONFIG_VAR(grenade_prediction_vars::TrajectoryHue)).toHueFloat();
        const auto bounceHue = static_cast<color::HueInteger>(GET_CONFIG_VAR(grenade_prediction_vars::BounceHue)).toHueFloat();
        renderer().draw(trajectory, panel, presentation, hookContext.hud().getHudReticle(), hue, bounceHue);
    }
    void renderCachedTrajectory(bool hasCurtime, float curtime) noexcept
    {
        auto& state = context().state();
        const bool present = state.liveGrenadeAuthority.hasAcceptedLiveProjectile() && !state.liveGrenadeAuthority.isFlashbangInEarlyHideWindow(hasCurtime ? Optional<float>{curtime} : Optional<float>{})
            && state.liveGrenadeCache.contains(state.liveGrenadeAuthority.acceptedLiveProjectile());
        const auto visibility = state.cacheVisibility(GET_CONFIG_VAR(grenade_prediction_vars::LastTrajectoryVisibility), GET_CONFIG_VAR(grenade_prediction_vars::CacheDuration), hasCurtime, curtime, present);
        if (visibility == LastGrenadeCacheVisibility::Show)
            draw(state.lastCommittedTrajectory, state.lastCacheContainerPanelHandle, state.lastCachePresentationState);
        else {
            if (visibility == LastGrenadeCacheVisibility::Invalidate) state.invalidateCommittedTrajectory();
            renderer().hide(state.lastCacheContainerPanelHandle);
        }
    }
    [[nodiscard]] bool acceptNewestLiveGrenade(cs2::C_CSPlayerPawn* pawn, bool hasCurtime, float curtime) noexcept
    {
        auto& state = context().state();
        const auto projectile = state.liveGrenadeAuthority.newestLocalProjectile(state.liveGrenadeCache);
        if (!projectile.hasValue() || !state.liveGrenadeAuthority.observeForSimulation(projectile.value()))
            return false;
        auto simulator = hookContext.template make<GrenadeSimulator>();
        state.liveGrenadeTrajectoryScratch.clear();
        GrenadePlayerCollisionSnapshotBuilder<HookContext>{hookContext}.build(state.playerCollisionSnapshot, pawn);
        simulator.setPlayerCollisionSnapshot(&state.playerCollisionSnapshot);
        simulator.simulate(state.liveGrenadeTrajectoryScratch, {projectile.value().initialPosition, projectile.value().initialVelocity}, projectile.value().kind, pawn,
            grenade_prediction::resolveServerGravity(hookContext.cvarSystem()));
        if (!state.liveGrenadeTrajectoryScratch.valid || !state.liveGrenadeTrajectoryScratch.pointsCount)
            return false;
        state.commitLiveGrenadeTrajectory();
        state.finalizeStagedTrajectory(true, hasCurtime, curtime);
        state.liveGrenadeAuthority.accept(projectile.value(), hasCurtime ? Optional<float>{curtime} : Optional<float>{});
        return true;
    }
    HookContext& hookContext;
};
