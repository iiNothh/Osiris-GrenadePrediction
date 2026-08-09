#pragma once

#include <Features/Visuals/GrenadePrediction/GrenadePredictionConfigVariables.h>
#include <GameClient/Panorama/PanoramaUiEngine.h>
#include <HookContext/HookContextMacros.h>

template <typename HookContext>
struct LastTrajectoryVisibilityDropdownSelectionChangeHandler {
    explicit LastTrajectoryVisibilityDropdownSelectionChangeHandler(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void onSelectionChanged(int selectedIndex) const noexcept
    {
        if (selectedIndex < 0 || selectedIndex > 3)
            return;

        SET_CONFIG_VAR(grenade_prediction_vars::LastTrajectoryVisibility,
            static_cast<grenade_prediction_vars::LastTrajectoryVisibilityMode>(selectedIndex));

        const auto mainMenuPointer = hookContext.patternSearchResults().template get<MainMenuPanelPointer>();
        auto&& mainMenu = hookContext.template make<ClientPanel>(mainMenuPointer ? *mainMenuPointer : nullptr).uiPanel();
        setDurationRowState(mainMenu, selectedIndex == 3);
    }

private:
    void setDurationRowState(auto&& mainMenu, bool enabled) const noexcept
    {
        auto&& row = mainMenu.findChildInLayoutFile("grenade_prediction_cache_duration_row");
        row.setOpacity(enabled ? 1.0f : 0.45f);
        row.setAttributeString(hookContext.template make<PanoramaUiEngine>().makeSymbol(0, "enabled"), enabled ? "true" : "false");
    }

    HookContext& hookContext;
};
