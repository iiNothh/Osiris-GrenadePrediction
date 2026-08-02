#pragma once

#include <CS2/Panorama/CPanel2D.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeTraceAdapter.h>
#include <Features/Visuals/GrenadePrediction/Held/HeldGrenadeLaunch.h>
#include <Features/Visuals/GrenadePrediction/Held/HeldGrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Rendering/GrenadePredictionRenderer.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeSimulationParameters.h>
#include <GameClient/EngineTrace/EngineTrace.h>
#include <GameClient/GrenadePrediction/GrenadeLaunch.h>
#include <GameClient/WorldToScreen/ViewToProjectionMatrix.h>
#include <MemoryPatterns/PatternTypes/ClientPatternTypes.h>

namespace grenade_prediction
{

template <typename HookContext>
class HeldGrenadePrediction {
public:
    HeldGrenadePrediction(HookContext& hookContext, HeldGrenadePredictionState& state) noexcept
        : hookContext{hookContext}
        , state{state}
    {
    }

    void run() noexcept
    {
        auto&& playerPawn = hookContext.activeLocalPlayerPawn();
        GrenadeLaunch<HookContext> nativeLaunch{hookContext};
        EngineTrace<HookContext> engineTrace{hookContext};
        GrenadeTraceAdapter trace{engineTrace};
        GrenadePredictionRenderer<HookContext> renderer{hookContext, state.rendererState};
        const auto hudPanel = hookContext.patternSearchResults().template get<HudPanelPointer>();
        if (!hudPanel || !*hudPanel || !(*hudPanel)->uiPanel) {
            clear(renderer);
            return;
        }
        runForPlayer(playerPawn, nativeLaunch, trace, renderer, (*hudPanel)->uiPanel, ViewToProjectionMatrix<HookContext>{hookContext}.getAspectRatio());
    }

    void clear() noexcept
    {
        GrenadePredictionRenderer<HookContext> renderer{hookContext, state.rendererState};
        clear(renderer);
    }

    template <typename PlayerPawn, typename NativeLaunchProvider, typename Trace, typename Renderer>
    void runForPlayer(const PlayerPawn& playerPawn, NativeLaunchProvider& nativeLaunch, Trace& trace, Renderer& renderer, cs2::CUIPanel* parent, float aspectRatio) noexcept
    {
        if (!playerPawn || !playerPawn.isAlive().value_or(false)) {
            clear(renderer);
            return;
        }
        const auto launch = HeldGrenadeLaunch{}.get(playerPawn, nativeLaunch);
        if (!launch.hasValue()) {
            clear(renderer);
            return;
        }
        GrenadeSimulator simulator{trace};
        simulator.simulate(GrenadeSimulationParameters{},
            {launch.value().launchState.origin, launch.value().launchState.velocity, launch.value().kind, playerPawn.raw()}, state.simulationResult);
        if (state.simulationResult.traceFailed || !state.simulationResult.trajectory.valid) {
            clear(renderer);
            return;
        }
        renderer.render(parent, state.simulationResult.trajectory, {}, aspectRatio);
    }

private:
    template <typename Renderer>
    void clear(Renderer& renderer) noexcept
    {
        renderer.clear();
        state.simulationResult.trajectory.reset({});
        state.simulationResult.traceFailed = false;
        state.simulationResult.detonated = false;
    }

    HookContext& hookContext;
    HeldGrenadePredictionState& state;
};

}
