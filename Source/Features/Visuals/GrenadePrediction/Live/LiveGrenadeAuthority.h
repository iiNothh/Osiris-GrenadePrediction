#pragma once

#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>
#include <Utils/Math.h>

enum class GrenadeTrajectoryAuthority { HeldPrediction, LiveProjectile };

class LiveGrenadeAuthority {
public:
    static constexpr float flashHorizon{1.5f + 0.125f};
    static constexpr float flashEarlyHideLead{0.020f};

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
        acceptedTime = 0.0f;
        accepted = false;
        hasAcceptedTime = false;
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

    void accept(const LiveGrenadeSnapshot& snapshot, Optional<float> currentTime = {}) noexcept
    {
        if (!LiveGrenadeCache::isValid(snapshot)) {
            reset();
            return;
        }

        acceptedSnapshot = snapshot;
        if (currentTime.hasValue() && Math::isFinite(currentTime.value())) {
            acceptedTime = currentTime.value();
            hasAcceptedTime = true;
        } else {
            acceptedTime = 0.0f;
            hasAcceptedTime = false;
        }
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

    void update(const LiveGrenadeCache& cache, Optional<float> currentTime = {}) noexcept
    {
        if (!accepted)
            return;

        if (!cache.contains(acceptedSnapshot) || isFlashbangInEarlyHideWindow(currentTime))
            reset();
    }

    [[nodiscard]] GrenadeTrajectoryAuthority trajectoryAuthority() const noexcept
    {
        return source;
    }

private:
    [[nodiscard]] bool isFlashbangInEarlyHideWindow(Optional<float> currentTime) const noexcept
    {
        return acceptedSnapshot.kind == cs2::GrenadeKind::Flashbang && hasAcceptedTime && currentTime.hasValue()
            && Math::isFinite(currentTime.value()) && currentTime.value() >= acceptedTime + flashHorizon - flashEarlyHideLead;
    }

    cs2::CEntityHandle localPawnHandle{};
    LiveGrenadeSnapshot acceptedSnapshot{};
    float acceptedTime{};
    bool hasLocalPawnHandle{};
    bool accepted{};
    bool hasAcceptedTime{};
    GrenadeTrajectoryAuthority source{GrenadeTrajectoryAuthority::HeldPrediction};
};
