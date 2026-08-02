#pragma once

#include <cstddef>

#include <CS2/Panorama/PanelHandle.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeTrajectory.h>

namespace grenade_prediction
{

struct GrenadePredictionRendererState {
    cs2::PanelHandle linePanels[GrenadeTrajectory::kPointCapacity]{};
    cs2::PanelHandle bouncePanels[GrenadeTrajectory::kBounceCapacity]{};
    cs2::PanelHandle endPanel{};
    std::size_t activeLineCount{};
    std::size_t activeBounceCount{};
    bool activeEndMarker{};

    void reset() noexcept
    {
        for (auto& handle : linePanels)
            handle = {};
        for (auto& handle : bouncePanels)
            handle = {};
        endPanel = {};
        activeLineCount = 0;
        activeBounceCount = 0;
        activeEndMarker = false;
    }
};

}
