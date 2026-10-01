#pragma once

struct GrenadeTrajectoryPanelStyleState {
    int lineSegmentCount{};
    int markersCount{};
    bool validLanding{};
    float trajectoryHue{};
    float bounceHue{};
    float trajectoryThickness{};
    bool initialized{};
};

struct GrenadeTrajectoryPresentationState {
    int activePanelCount{};
    GrenadeTrajectoryPanelStyleState panelStyle{};
};
