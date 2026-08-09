#pragma once

#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>

enum class GrenadeTrajectoryAuthority { HeldPrediction, LiveProjectile };

class LiveGrenadeAuthority {
public:
    void observeLocalPawn(cs2::CEntityHandle handle) noexcept
    {
        if (!hasLocalPawnHandle || localPawnHandle != handle) {
            localPawnHandle = handle;
            hasLocalPawnHandle = true;
            reset();
        }
    }

    void reset() noexcept
    {
        acceptedSnapshot = {};
        accepted = false;
        source = GrenadeTrajectoryAuthority::HeldPrediction;
    }

    [[nodiscard]] Optional<LiveGrenadeSnapshot> newestLocalProjectile(const LiveGrenadeCache& cache) const noexcept
    {
        if (!hasLocalPawnHandle)
            return {};
        return cache.newestForThrower(localPawnHandle);
    }

    [[nodiscard]] bool shouldAdopt(const LiveGrenadeSnapshot& snapshot) const noexcept
    {
        return !accepted || snapshot.firstObservationSequence != acceptedSnapshot.firstObservationSequence;
    }

    void accept(const LiveGrenadeSnapshot& snapshot) noexcept
    {
        acceptedSnapshot = snapshot;
        accepted = true;
        source = GrenadeTrajectoryAuthority::LiveProjectile;
    }

    [[nodiscard]] const LiveGrenadeSnapshot& acceptedLiveProjectile() const noexcept
    {
        return acceptedSnapshot;
    }

    [[nodiscard]] bool hasAcceptedLiveProjectile() const noexcept
    {
        return accepted;
    }

    [[nodiscard]] GrenadeTrajectoryAuthority trajectoryAuthority() const noexcept
    {
        return source;
    }

private:
    cs2::CEntityHandle localPawnHandle{};
    LiveGrenadeSnapshot acceptedSnapshot{};
    bool hasLocalPawnHandle{};
    bool accepted{};
    GrenadeTrajectoryAuthority source{GrenadeTrajectoryAuthority::HeldPrediction};
};
