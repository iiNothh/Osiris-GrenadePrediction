#pragma once

#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeAuthorityState.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>
#include <Features/Visuals/GrenadePrediction/Rendering/GrenadePredictionRenderer.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeSimulationParameters.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeSimulator.h>

namespace grenade_prediction
{

template <typename HookContext>
class LiveGrenadePrediction {
public:
    LiveGrenadePrediction(HookContext& hookContext, LiveGrenadeCacheState& cacheState, LiveGrenadeAuthorityState& authorityState) noexcept
        : hookContext{hookContext}
        , cache{cacheState}
        , state{authorityState}
    {
    }

    void beginScan() noexcept { cache.beginScan(); }
    void endScan() noexcept { cache.endScan(); }

    void observe(const LiveGrenadeSnapshot& snapshot) noexcept { cache.observe(snapshot); }
    void remove(cs2::CEntityHandle handle) noexcept { cache.remove(handle); }

    template <typename Trace>
    [[nodiscard]] bool run(void* rawLocalPawn, cs2::CEntityHandle localPawnHandle, Trace& trace, cs2::CUIPanel* parent, float aspectRatio) noexcept
    {
        GrenadePredictionRenderer<HookContext> renderer{hookContext, state.rendererState};
        return runForLocalPawn(rawLocalPawn, localPawnHandle, trace, renderer, parent, aspectRatio);
    }

    template <typename Trace, typename Renderer>
    [[nodiscard]] bool runForLocalPawn(void* rawLocalPawn, cs2::CEntityHandle localPawnHandle, Trace& trace, Renderer& renderer, cs2::CUIPanel* parent, float aspectRatio) noexcept
    {
        if (!cache.scanCompleted()) {
            renderer.clear();
            return false;
        }
        if (!rawLocalPawn) {
            clearAccepted(renderer);
            return false;
        }
        if (state.localPawnIdentity != localPawnHandle) {
            clearAccepted(renderer);
            state.localPawnIdentity = localPawnHandle;
            state.watermark = 0;
        }

        const auto* candidate = cache.newestForThrower(localPawnHandle);
        if (!candidate) {
            clearAccepted(renderer);
            return false;
        }
        if (state.accepted && candidate->handle == state.acceptedProjectileHandle
            && candidate->thrower == state.acceptedThrowerHandle
            && candidate->firstObservationSequence == state.acceptedFirstObservationSequence
            && cache.contains(state.acceptedProjectileHandle, state.acceptedThrowerHandle, state.acceptedFirstObservationSequence)) {
            renderer.render(parent, state.acceptedTrajectory, {}, aspectRatio);
            return true;
        }
        if (candidate->firstObservationSequence < state.watermark) {
            clearAccepted(renderer);
            return false;
        }

        state.watermark = candidate->firstObservationSequence;
        GrenadeSimulator simulator{trace};
        simulator.simulate(GrenadeSimulationParameters{}, {candidate->initialPosition, candidate->initialVelocity, candidate->kind, rawLocalPawn}, state.candidateSimulationResult);
        if (state.candidateSimulationResult.traceFailed || !state.candidateSimulationResult.trajectory.valid || state.candidateSimulationResult.trajectory.pointCount == 0) {
            clearAccepted(renderer);
            return false;
        }

        state.acceptedTrajectory = state.candidateSimulationResult.trajectory;
        state.acceptedProjectileHandle = candidate->handle;
        state.acceptedThrowerHandle = candidate->thrower;
        state.acceptedFirstObservationSequence = candidate->firstObservationSequence;
        state.accepted = true;
        renderer.render(parent, state.acceptedTrajectory, {}, aspectRatio);
        return true;
    }

    void clear() noexcept
    {
        GrenadePredictionRenderer<HookContext>{hookContext, state.rendererState}.clear();
        cache.clear();
        clearAcceptedState();
        state.watermark = 0;
        state.localPawnIdentity = {};
    }

    void clearAuthority() noexcept
    {
        GrenadePredictionRenderer<HookContext> renderer{hookContext, state.rendererState};
        clearAccepted(renderer);
    }

private:
    void clearAcceptedState() noexcept
    {
        state.accepted = false;
        state.acceptedProjectileHandle = {};
        state.acceptedThrowerHandle = {};
        state.acceptedFirstObservationSequence = 0;
        state.acceptedTrajectory.reset({});
        state.candidateSimulationResult.trajectory.reset({});
        state.candidateSimulationResult.traceFailed = false;
        state.candidateSimulationResult.detonated = false;
    }

    template <typename Renderer>
    void clearAccepted(Renderer& renderer) noexcept
    {
        renderer.clear();
        clearAcceptedState();
    }

    HookContext& hookContext;
    LiveGrenadeCache cache;
    LiveGrenadeAuthorityState& state;
};

}
