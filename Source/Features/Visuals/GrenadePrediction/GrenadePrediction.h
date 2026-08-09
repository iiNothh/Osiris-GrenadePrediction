#pragma once

#include <type_traits>

#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Classes/Entities/WeaponEntities.h>
#include <Features/Visuals/GrenadePrediction/GrenadeGravity.h>
#include <Features/Visuals/GrenadePrediction/GrenadeKindMapper.h>
#include <Features/Visuals/GrenadePrediction/GrenadeLaunchSelection.h>
#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionSnapshotBuilder.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionConfigVariables.h>
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
#include <MemoryPatterns/PatternTypes/WeaponPatternTypes.h>

template <typename HookContext>
class GrenadePrediction {
public:
    explicit GrenadePrediction(HookContext& hookContext) noexcept : hookContext{hookContext} {}

    void beginLiveGrenadeScan() noexcept { LiveGrenadeCacheUpdater{context().state().liveGrenadeCache}.beginScan(); }
    void endLiveGrenadeScan(cs2::C_CSPlayerPawn* localPawn, cs2::CEntityHandle localPawnHandle) noexcept
    {
        auto& state = context().state();
        LiveGrenadeCacheUpdater{state.liveGrenadeCache}.endScan();
        state.liveGrenadeAuthority.observeLocalPawn(localPawnHandle);
        const auto currentTime = hookContext.globalVars().curtime();
        state.liveGrenadeAuthority.update(state.liveGrenadeCache, currentTime);

        const auto projectile = state.liveGrenadeAuthority.newestLocalProjectile(state.liveGrenadeCache);
        if (!projectile.hasValue() || !state.liveGrenadeAuthority.shouldAdopt(projectile.value()))
            return;

        auto gravity = grenade_prediction::serverGravity(hookContext.cvarSystem());
        if (!gravity.hasValue())
            return;

        GrenadePlayerCollisionSnapshotBuilder<HookContext>{hookContext}.build(state.playerCollisionSnapshot, localPawn);
        auto simulator = hookContext.template make<GrenadeSimulator>();
        simulator.setPlayerCollisionSnapshot(&state.playerCollisionSnapshot);
        simulator.simulate(state.liveGrenadeTrajectoryScratch, {projectile.value().initialPosition, projectile.value().initialVelocity}, projectile.value().kind,
            localPawn, gravity.value());
        if (!state.liveGrenadeTrajectoryScratch.valid || !state.liveGrenadeTrajectoryScratch.pointsCount)
            return;

        state.commitLiveTrajectory(currentTime.valueOr(0.0f), currentTime.hasValue());
        state.liveGrenadeAuthority.accept(projectile.value(), currentTime);
    }

    void updateLiveGrenade(const cs2::CEntityIdentity& identity, EntityTypeInfo type) noexcept
    {
        const auto kind = GrenadeKindMapper::fromProjectile(type);
        if (kind == cs2::GrenadeKind::None || !identity.entity)
            return;
        const auto grenade = GrenadeProjectile{hookContext, static_cast<cs2::C_BaseCSGrenadeProjectile*>(identity.entity)};
        LiveGrenadeLifecycleState lifecycleState;
        if (kind == cs2::GrenadeKind::SmokeGrenade)
            lifecycleState.smokeEffectStarted = SmokeGrenadeProjectile{hookContext, static_cast<cs2::C_SmokeGrenadeProjectile*>(identity.entity)}.didSmokeEffect();
        else if (kind == cs2::GrenadeKind::Decoy)
            lifecycleState.decoyShotTick = DecoyProjectile{hookContext, static_cast<cs2::C_DecoyProjectile*>(identity.entity)}.decoyShotTick();
        static_cast<void>(LiveGrenadeCacheUpdater{context().state().liveGrenadeCache}.update(grenade, identity.handle, kind, lifecycleState));
    }

    void handleGrenadePrediction(auto&& playerPawn, auto&& activeWeapon, cs2::CEntityHandle localPawnHandle) noexcept
    {
        auto& state = context().state();
        if (!shouldRun(playerPawn)) { clearPrediction(); return; }
        state.liveGrenadeAuthority.observeLocalPawn(localPawnHandle);
        const auto curtime = hookContext.globalVars().curtime();
        const bool hasCurtime = curtime.hasValue();
        const float time = curtime.valueOr(0.0f);
        state.liveGrenadeAuthority.update(state.liveGrenadeCache, curtime);
        renderCachedTrajectory(hasCurtime, time);

        const auto kind = GrenadeKindMapper::from(activeWeapon.baseEntity().classify());
        auto* const weapon = static_cast<cs2::C_BaseCSGrenade*>(static_cast<cs2::C_BaseEntity*>(activeWeapon.baseEntity()));
        auto* const pawn = static_cast<cs2::C_CSPlayerPawn*>(static_cast<cs2::C_BaseEntity*>(playerPawn.baseEntity()));
        if (!weapon || kind == cs2::GrenadeKind::None) { hideLive(); return; }

        observeHeldThrow(weapon);
        if (state.throwObservation.consumeActualExecution(hasCurtime, time)) {
            if (state.ownsTempTrajectory(weapon, state.throwObservation.pendingSequence()))
                state.commitTempTrajectory(time, hasCurtime);
            state.invalidateTempTrajectory();
            hideLive();
            return;
        }

        const bool shouldUpdate = state.updateScheduler.shouldUpdate(false, hookContext.globalVars().frametime().hasValue(), hookContext.globalVars().frametime().valueOr(0.0f));
        if (!shouldUpdate) return;
        const auto launch = prepareGrenadeLaunch(false, true, state.throwObservation.isFinalized(), true, [&]() noexcept {
            return hookContext.template make<GrenadeLaunch<HookContext>>().get(weapon, pawn);
        });
        if (launch.status != GrenadeLaunchPreparationStatus::Ready) { hideLive(); return; }

        auto gravity = grenade_prediction::serverGravity(hookContext.cvarSystem());
        if (!gravity.hasValue()) { hideLive(); return; }
        auto simulator = hookContext.template make<GrenadeSimulator>();
        GrenadePlayerCollisionSnapshotBuilder<HookContext>{hookContext}.build(state.playerCollisionSnapshot, pawn);
        simulator.setPlayerCollisionSnapshot(&state.playerCollisionSnapshot);
        simulator.simulate(state.tempTrajectory, launch.state.value(), kind, pawn, gravity.value());
        if (!state.tempTrajectory.valid || !state.tempTrajectory.pointsCount) { state.invalidateTempTrajectory(); hideLive(); return; }
        state.tagTempTrajectory(weapon, state.throwObservation.sequence);
        draw(state.tempTrajectory, state.liveContainerPanelHandle, state.livePresentationState);
    }

    void clearPrediction() noexcept
    {
        auto& state = context().state();
        state.throwObservation.reset();
        state.updateScheduler.reset();
        state.liveGrenadeAuthority.reset();
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
    [[nodiscard]] bool shouldRun(const auto& playerPawn) const noexcept
    {
        return GET_CONFIG_VAR(grenade_prediction_vars::Enabled) && playerPawn.isControlledByLocalPlayer() && playerPawn.isAlive().value_or(false);
    }
    [[nodiscard]] decltype(auto) context() const noexcept { return hookContext.template make<GrenadePredictionContext>(); }
    [[nodiscard]] auto renderer() noexcept { return hookContext.template make<GrenadeTrajectoryRenderer>(); }
    void hideLive() noexcept { renderer().hide(context().state().liveContainerPanelHandle); }
    void observeHeldThrow(cs2::C_BaseCSGrenade* weapon) noexcept
    {
        auto& observation = context().state().throwObservation;
        static_cast<void>(observation.observeWeapon(weapon));
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<OffsetToPinPulled>()) {
            const auto pinPulled = hookContext.patternSearchResults().template get<OffsetToPinPulled>().of(weapon).toOptional();
            if (pinPulled.hasValue())
                static_cast<void>(observation.observePinState(weapon, pinPulled.value()));
        }
        if constexpr (std::remove_cvref_t<decltype(hookContext.patternSearchResults())>::template supports<OffsetToThrowTime>()) {
            const auto throwTime = hookContext.patternSearchResults().template get<OffsetToThrowTime>().of(weapon).toOptional();
            if (throwTime.hasValue())
                static_cast<void>(observation.observeThrowTime(weapon, throwTime.value()));
        }
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
        const bool present = state.liveGrenadeAuthority.hasAcceptedLiveProjectile() && state.liveGrenadeCache.contains(state.liveGrenadeAuthority.acceptedLiveProjectile());
        const auto visibility = state.cacheVisibility(GET_CONFIG_VAR(grenade_prediction_vars::LastTrajectoryVisibility), GET_CONFIG_VAR(grenade_prediction_vars::CacheDuration), hasCurtime, curtime, present);
        if (visibility == LastGrenadeCacheVisibility::Show)
            draw(state.lastCommittedTrajectory, state.lastCacheContainerPanelHandle, state.lastCachePresentationState);
        else {
            if (visibility == LastGrenadeCacheVisibility::Invalidate) state.invalidateCommittedTrajectory();
            renderer().hide(state.lastCacheContainerPanelHandle);
        }
    }
    HookContext& hookContext;
};
