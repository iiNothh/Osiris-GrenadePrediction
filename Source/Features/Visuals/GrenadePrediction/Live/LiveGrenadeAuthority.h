#pragma once

#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>

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
        highestObservedObservationSequence = 0;
        acceptedTime = 0.0f;
        accepted = false;
        hasHighestObserved = false;
        hasAcceptedTime = false;
        source = GrenadeTrajectoryAuthority::HeldPrediction;
    }

    [[nodiscard]] Optional<LiveGrenadeSnapshot> newestLocalProjectile(const LiveGrenadeCache& cache) const noexcept
    {
        if (!hasLocalPawnHandle)
            return {};
        return cache.newestForThrower(localPawnHandle);
    }

    [[nodiscard]] bool observeForSimulation(const LiveGrenadeSnapshot& snapshot) noexcept
    {
        if (!hasHighestObserved || snapshot.observationSequence > highestObservedObservationSequence) {
            highestObservedObservationSequence = snapshot.observationSequence;
            hasHighestObserved = true;
            accepted = false;
            hasAcceptedTime = false;
            source = GrenadeTrajectoryAuthority::HeldPrediction;
            return true;
        }
        return snapshot.observationSequence == highestObservedObservationSequence
            && (!accepted || snapshot.observationSequence != acceptedSnapshot.observationSequence);
    }

    void accept(const LiveGrenadeSnapshot& snapshot, Optional<float> currentTime = {}) noexcept
    {
        acceptedSnapshot = snapshot;
        if (currentTime.hasValue()) {
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
public:
    [[nodiscard]] bool isFlashbangInEarlyHideWindow(Optional<float> currentTime) const noexcept
    {
        return acceptedSnapshot.kind == cs2::GrenadeKind::Flashbang && hasAcceptedTime && currentTime.hasValue()
            && currentTime.value() >= acceptedTime + flashHorizon - flashEarlyHideLead;
    }
    [[nodiscard]] bool hasObservedLiveProjectile() const noexcept { return hasHighestObserved; }

private:
    cs2::CEntityHandle localPawnHandle{};
    LiveGrenadeSnapshot acceptedSnapshot{};
    float acceptedTime{};
    bool hasLocalPawnHandle{};
    bool accepted{};
    bool hasHighestObserved{};
    bool hasAcceptedTime{};
    std::uint32_t highestObservedObservationSequence{};
    GrenadeTrajectoryAuthority source{GrenadeTrajectoryAuthority::HeldPrediction};
};
