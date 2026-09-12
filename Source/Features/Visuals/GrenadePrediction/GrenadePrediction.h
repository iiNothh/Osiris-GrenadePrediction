#pragma once

#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Classes/Entities/WeaponEntities.h>
#include <CS2/Classes/EntitySystem/CEntityIdentity.h>
#include <GameClient/Entities/EntityClassifier.h>
#include <GameClient/Entities/GrenadeKind.h>
#include <GameClient/Entities/GrenadeKindMapper.h>
#include <GameClient/Entities/GrenadeWeapon.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeLaunchSelection.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeLaunchFallback.h>
#include <Features/Visuals/GrenadePrediction/CachedGrenadeTrajectoryPresentation.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionConfigVariables.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Rendering/GrenadeTrajectoryRenderer.h>
#include <Features/Visuals/GrenadePrediction/GrenadeTracePreset.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadePrediction.h>
#include <GameClient/Entities/PlayerPawn.h>
#include <GameClient/EngineTrace/EngineTrace.h>
#include <GameClient/GlobalVars.h>
#include <GameClient/GrenadePrediction/GrenadeLaunch.h>
#include <GameClient/Panorama/PanoramaUiEngine.h>
#include <HookContext/HookContextMacros.h>
#include <Platform/GrenadePredictionCapabilities.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

template <typename HookContext>
class GrenadePrediction {
public:
    explicit GrenadePrediction(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void beginLiveGrenadeScan() noexcept
    {
        LiveGrenadePrediction<HookContext>{hookContext, state()}.beginScan();
    }

    void endLiveGrenadeScan(cs2::C_CSPlayerPawn* localPawn) noexcept
    {
        LiveGrenadePrediction<HookContext>{hookContext, state()}.endScan(localPawn);
    }

    void updateLiveGrenade(const cs2::CEntityIdentity& identity, EntityTypeInfo type) noexcept
    {
        LiveGrenadePrediction<HookContext>{hookContext, state()}.observeEntity(identity, type);
    }

    void handleGrenadePrediction(auto&& playerPawn, auto&& activeWeapon, cs2::CEntityHandle localPawnHandle, bool enabled) noexcept
    {
        if (!playerPawn.isControlledByLocalPlayer())
            return;
        auto& state = this->state();
        state.beginFrame();
        const Optional<float> curtime = finiteTime(hookContext.globalVars().curtime());
        const bool hasCurtime = curtime.hasValue();
        const float time = curtime.valueOr(0.0f);
        auto* const pawn = static_cast<cs2::C_CSPlayerPawn*>(static_cast<cs2::C_BaseEntity*>(playerPawn.baseEntity()));
        if constexpr (!GrenadePredictionPlatformCapabilities::supportsHeldPrediction) {
            clearPrediction();
            applyCachedTrajectoryPresentation(hasCurtime, time);
            return;
        } else {
            if (!grenade_trace_preset::isInFlightTraceAvailable(hookContext.template make<EngineTrace>())) {
                clearPrediction();
                return;
            }
            if (!playerPawn.isAlive().value_or(false) || !enabled) {
                clearPrediction();
                applyCachedTrajectoryPresentation(hasCurtime, time);
                return;
            }
            if constexpr (GrenadePredictionPlatformCapabilities::supportsLiveProjectilePrediction) {
                auto& liveGrenadeTrajectoryScratch = hookContext.grenadePredictionPerHookState().liveGrenadeTrajectoryScratch;
                if (LiveGrenadePrediction<HookContext>{hookContext, state}.attemptNewestProjectileAdoption(liveGrenadeTrajectoryScratch, localPawnHandle, curtime, pawn))
                    hideLivePrediction();
            }
            static_cast<void>(state.observeTime(hasCurtime, time));
            const Optional<float> frametime = finiteTime(hookContext.globalVars().frametime());
            bool shouldUpdate = state.updateScheduler.shouldUpdate(false, frametime.hasValue(), frametime.valueOr(0.0f));
            const auto pendingWeapon = state.throwObservation.pendingWeapon();
            if (state.completeHeldThrow(pendingWeapon, hasCurtime, time)) {
                hideLivePrediction();
                applyCachedTrajectoryPresentation(hasCurtime, time);
                return;
            }
            const auto kind = GrenadeKindMapper::from(activeWeapon.baseEntity().classify());
            auto* const weapon = static_cast<cs2::C_BaseCSGrenade*>(static_cast<cs2::C_BaseEntity*>(activeWeapon.baseEntity()));
            const auto weaponHandle = activeWeapon.baseEntity().handle();
            if (!isUsableHeldGrenade(weapon, kind, weaponHandle)) {
                state.throwObservation.reset();
                state.invalidateTempTrajectory();
                hideLivePrediction();
                applyCachedTrajectoryPresentation(hasCurtime, time);
                return;
            }

            const Optional<float> throwTime = finiteTime(hookContext.template make<GrenadeWeapon>(weapon).throwTime());
            const bool releaseEdge = observeHeldThrow(weapon, weaponHandle, throwTime);
            const bool scheduledTransition = throwTime.hasValue() && state.throwObservation.observeThrowTime(weaponHandle, throwTime.value());
            if (state.completeHeldThrow(state.throwObservation.pendingWeapon(), hasCurtime, time)
                || (!throwTime.hasValue() && state.completeLegacyHeldThrow(weaponHandle, releaseEdge, hasCurtime, time))) {
                hideLivePrediction();
                applyCachedTrajectoryPresentation(hasCurtime, time);
                return;
            }
            if (scheduledTransition)
                shouldUpdate = state.updateScheduler.shouldUpdate(true, frametime.hasValue(), frametime.valueOr(0.0f));
            if (!shouldUpdate) {
                presentHeldTrajectory(weaponHandle, hasCurtime, time);
                return;
            }
            auto simulator = hookContext.template make<GrenadeSimulator>();
            const auto launch = prepareHeldGrenadeLaunch(playerPawn, weapon, pawn, simulator);
            if (!launch.hasValue()) {
                hideLivePrediction(state.throwObservation.hasPendingExecution());
                if (!state.throwObservation.hasPendingExecution())
                    state.invalidateTempTrajectory();
                applyCachedTrajectoryPresentation(hasCurtime, time);
                return;
            }

            const auto gravity = grenade_prediction::resolveServerGravity(hookContext.cvarSystem());
            const HeldGrenadeSimulationInput input{launch.value().origin, launch.value().velocity, kind, gravity, state.playerCollisionSnapshot.revision,
                localPawnHandle, weaponHandle, state.throwObservation.sequence};
            simulateHeldTrajectoryIfNeeded(simulator, launch.value(), kind, pawn, gravity, input);
            presentHeldTrajectory(weaponHandle, hasCurtime, time);
        }
    }

    void handleNoLocalPawn() noexcept
    {
        auto& state = this->state();
        state.beginFrame();
        if constexpr (GrenadePredictionPlatformCapabilities::supportsHeldPrediction) {
            if (!grenade_trace_preset::isInFlightTraceAvailable(hookContext.template make<EngineTrace>())) {
                clearPrediction();
                return;
            }
        }
        const Optional<float> curtime = finiteTime(hookContext.globalVars().curtime());
        static_cast<void>(state.observeTime(curtime.hasValue(), curtime.valueOr(0.0f)));
        clearPrediction();
        applyCachedTrajectoryPresentation(curtime.hasValue(), curtime.valueOr(0.0f));
    }

    void clearPrediction() noexcept
    {
        auto& state = this->state();
        state.clearPrediction();
        hideLivePrediction();
        renderer().hide(state.lastCacheContainerPanelHandle);
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
        state.resetPresentationState();
    }

private:
    [[nodiscard]] auto& state() const noexcept
    {
        return hookContext.featuresStates().visualFeaturesStates.grenadePredictionState;
    }

    [[nodiscard]] auto renderer() noexcept
    {
        return hookContext.template make<GrenadeTrajectoryRenderer>();
    }

    [[nodiscard]] static Optional<float> finiteTime(Optional<float> time) noexcept
    {
        if (!time.hasValue() || !Math::isFinite(time.value()))
            return {};
        return time;
    }

    [[nodiscard]] static bool isUsableHeldGrenade(cs2::C_BaseCSGrenade* weapon, GrenadeKind kind, cs2::CEntityHandle weaponHandle) noexcept
    {
        return weapon && kind != GrenadeKind::None && weaponHandle != cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX};
    }

    void hideLivePrediction(bool preserve = false) noexcept
    {
        auto& state = this->state();
        renderer().hide(state.liveContainerPanelHandle);
        if (!preserve)
            state.invalidateTempTrajectory();
    }

    [[nodiscard]] Optional<GrenadeLaunchState> prepareHeldGrenadeLaunch(auto&& playerPawn, cs2::C_BaseCSGrenade* weapon,
        cs2::C_CSPlayerPawn* pawn, auto& simulator) noexcept
    {
        const auto& observation = state().throwObservation;
        return prepareGrenadeLaunch(observation.isFinalized(), observation.hasRetainedThrowStrength,
            [&]() noexcept { return hookContext.template make<GrenadeLaunch<HookContext>>().get(weapon, pawn); },
            [&]() noexcept { return computeHeldGrenadeLaunchFallback(playerPawn, simulator, observation.retainedThrowStrength, pawn); });
    }

    void simulateHeldTrajectoryIfNeeded(auto& simulator, const GrenadeLaunchState& launch, GrenadeKind kind, cs2::C_CSPlayerPawn* pawn,
        float gravity, const HeldGrenadeSimulationInput& input) noexcept
    {
        auto& state = this->state();
        if (!state.liveGrenadeAuthority.blocksHeldPrediction() && state.shouldSimulateHeld(input)) {
            simulator.setPlayerCollisionSnapshot(&state.playerCollisionSnapshot);
            simulator.simulate(state.tempTrajectory, launch, kind, pawn, gravity);
            const bool succeeded = state.tempTrajectory.valid && state.tempTrajectory.pointsCount;
            state.recordHeldSimulationResult(input, succeeded);
            if (!succeeded)
                hideLivePrediction(state.throwObservation.hasPendingExecution());
        }
    }

    void presentHeldTrajectory(cs2::CEntityHandle weaponHandle, bool hasCurtime, float curtime) noexcept
    {
        auto& state = this->state();
        if (!state.liveGrenadeAuthority.blocksHeldPrediction() && state.ownsTempTrajectory(weaponHandle, state.throwObservation.sequence))
            drawTrajectory(state.tempTrajectory, state.liveContainerPanelHandle, state.livePresentationState);
        else if (state.liveGrenadeAuthority.blocksHeldPrediction())
            hideLivePrediction();
        applyCachedTrajectoryPresentation(hasCurtime, curtime);
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
            releaseEdge = observation.observePinState(weaponHandle, pinPulled);
        }
        observation.captureThrowStrength(pinPulled, throwTime, [&]() noexcept {
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
        const auto decision = CachedGrenadeTrajectoryPresentation::decide(state, GET_CONFIG_VAR(grenade_prediction_vars::LastTrajectoryVisibility), GET_CONFIG_VAR(grenade_prediction_vars::CacheDuration),
            hasCurtime, curtime, acceptedProjectilePresent(state, hasCurtime, curtime));
        CachedGrenadeTrajectoryPresentation::apply(state, decision,
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
