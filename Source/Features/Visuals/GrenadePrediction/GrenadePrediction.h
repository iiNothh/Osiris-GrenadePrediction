#pragma once

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
    void endLiveGrenadeScan() noexcept { LiveGrenadeCacheUpdater{context().state().liveGrenadeCache}.endScan(); }

    void updateLiveGrenade(const cs2::CEntityIdentity& identity, EntityTypeInfo type) noexcept
    {
        const auto kind = GrenadeKindMapper::fromProjectile(type);
        if (kind == cs2::GrenadeKind::None || !identity.entity)
            return;
        if (kind == cs2::GrenadeKind::SmokeGrenade) {
            const auto smoke = SmokeGrenadeProjectile{hookContext, static_cast<cs2::C_SmokeGrenadeProjectile*>(identity.entity)}.didSmokeEffect();
            if (!smoke.hasValue() || smoke.value())
                return;
        }
        const auto grenade = GrenadeProjectile{hookContext, static_cast<cs2::C_BaseCSGrenadeProjectile*>(identity.entity)};
        static_cast<void>(LiveGrenadeCacheUpdater{context().state().liveGrenadeCache}.update(grenade, identity.handle, kind));
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

        const bool shouldUpdate = state.updateScheduler.shouldUpdate(false, hookContext.globalVars().frametime().hasValue(), hookContext.globalVars().frametime().valueOr(0.0f));
        if (!shouldUpdate) return;
        const auto launch = prepareGrenadeLaunch(false, true, false, true, [&]() noexcept {
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
