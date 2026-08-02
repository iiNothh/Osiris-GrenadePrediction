#pragma once

#include <cstdint>

#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeSnapshot.h>
#include <Utils/DynamicArray.h>

namespace grenade_prediction
{

struct LiveGrenadeCacheState {
    DynamicArray<LiveGrenadeSnapshot> snapshots;
    std::uint64_t nextFirstObservationSequence{1};
    bool scanCompleted{};
};

}
