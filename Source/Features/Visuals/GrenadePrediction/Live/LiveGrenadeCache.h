#pragma once

#include <cstddef>

#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheState.h>

namespace grenade_prediction
{

class LiveGrenadeCache {
public:
    explicit LiveGrenadeCache(LiveGrenadeCacheState& state) noexcept : state{state} {}

    void beginScan() noexcept
    {
        state.scanCompleted = false;
        for (auto& snapshot : state.snapshots)
            snapshot.seen = false;
    }

    void observe(const LiveGrenadeSnapshot& snapshot) noexcept
    {
        if (auto* existing = find(snapshot.handle)) {
            const auto sequence = existing->firstObservationSequence;
            *existing = snapshot;
            existing->firstObservationSequence = sequence;
            existing->seen = true;
            return;
        }
        auto newSnapshot = snapshot;
        newSnapshot.firstObservationSequence = state.nextFirstObservationSequence;
        newSnapshot.seen = true;
        if (state.snapshots.pushBack(newSnapshot))
            ++state.nextFirstObservationSequence;
    }

    void remove(cs2::CEntityHandle handle) noexcept
    {
        for (std::size_t i = 0; i < state.snapshots.getSize(); ++i) {
            if (state.snapshots[i].handle == handle) {
                state.snapshots.fastRemoveAt(i);
                return;
            }
        }
    }

    void endScan() noexcept
    {
        for (std::size_t i = 0; i < state.snapshots.getSize();) {
            if (!state.snapshots[i].seen)
                state.snapshots.fastRemoveAt(i);
            else
                ++i;
        }
        state.scanCompleted = true;
    }

    [[nodiscard]] const LiveGrenadeSnapshot* find(cs2::CEntityHandle handle) const noexcept
    {
        for (const auto& snapshot : state.snapshots) {
            if (snapshot.handle == handle)
                return &snapshot;
        }
        return nullptr;
    }

    [[nodiscard]] const LiveGrenadeSnapshot* newestForThrower(cs2::CEntityHandle throwerHandle) const noexcept
    {
        const LiveGrenadeSnapshot* newest{};
        for (const auto& snapshot : state.snapshots) {
            if (snapshot.thrower != throwerHandle)
                continue;
            if (!newest || snapshot.firstObservationSequence > newest->firstObservationSequence)
                newest = &snapshot;
        }
        return newest;
    }

    [[nodiscard]] bool contains(cs2::CEntityHandle projectileHandle, cs2::CEntityHandle throwerHandle, std::uint64_t firstObservationSequence) const noexcept
    {
        for (const auto& snapshot : state.snapshots) {
            if (snapshot.handle == projectileHandle && snapshot.thrower == throwerHandle && snapshot.firstObservationSequence == firstObservationSequence)
                return true;
        }
        return false;
    }

    [[nodiscard]] const DynamicArray<LiveGrenadeSnapshot>& snapshots() const noexcept { return state.snapshots; }
    [[nodiscard]] bool scanCompleted() const noexcept { return state.scanCompleted; }
    void clear() noexcept { state.snapshots.clear(); state.nextFirstObservationSequence = 1; state.scanCompleted = false; }

private:
    [[nodiscard]] LiveGrenadeSnapshot* find(cs2::CEntityHandle handle) noexcept
    {
        for (auto& snapshot : state.snapshots) {
            if (snapshot.handle == handle)
                return &snapshot;
        }
        return nullptr;
    }

    LiveGrenadeCacheState& state;
};

}
