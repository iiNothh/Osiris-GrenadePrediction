#pragma once

#include <cstdint>

#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>

class LiveGrenadeAuthority {
public:
    static constexpr float flashHorizon{1.5f + 0.125f};
    static constexpr float flashEarlyHideLead{0.020f};
    static constexpr std::uint32_t maxSimulationRetryDelayFrames{32};

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
        newestObservedSnapshot = {};
        highestObservedObservationSequence = 0;
        acceptedTime = 0.0f;
        accepted = false;
        hasHighestObserved = false;
        hasAcceptedTime = false;
        resetSimulationRetryBackoff();
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
            newestObservedSnapshot = snapshot;
            hasHighestObserved = true;
            accepted = false;
            hasAcceptedTime = false;
            resetSimulationRetryBackoff();
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
        resetSimulationRetryBackoff();
    }

    [[nodiscard]] const LiveGrenadeSnapshot& acceptedLiveProjectile() const noexcept
    {
        return acceptedSnapshot;
    }

    [[nodiscard]] bool hasAcceptedLiveProjectile() const noexcept
    {
        return accepted;
    }

    [[nodiscard]] bool isSimulationRetryDue(const LiveGrenadeSnapshot& snapshot, std::uint32_t frame) const noexcept
    {
        return !hasRetryObservationSequence || snapshot.observationSequence != retryObservationSequence || !hasScheduledRetry
            || frame - nextSimulationRetryFrame < (std::uint32_t{1} << 31);
    }

    void recordSimulationFailure(const LiveGrenadeSnapshot& snapshot, std::uint32_t frame) noexcept
    {
        if (!hasRetryObservationSequence || snapshot.observationSequence != retryObservationSequence)
            resetSimulationRetryBackoff();

        retryObservationSequence = snapshot.observationSequence;
        hasRetryObservationSequence = true;
        nextSimulationRetryFrame = frame + nextSimulationRetryDelayFrames;
        hasScheduledRetry = true;
        if (nextSimulationRetryDelayFrames < maxSimulationRetryDelayFrames)
            nextSimulationRetryDelayFrames *= 2;
    }

    void update(const LiveGrenadeCache& cache) noexcept
    {
        if (!cache.isScanComplete())
            return;

        if ((hasHighestObserved && !cache.contains(newestObservedSnapshot)) || (accepted && !cache.contains(acceptedSnapshot))) {
            reset();
            return;
        }

    }
    [[nodiscard]] bool isFlashbangInEarlyHideWindow(Optional<float> currentTime) const noexcept
    {
        return acceptedSnapshot.kind == cs2::GrenadeKind::Flashbang && hasAcceptedTime && currentTime.hasValue()
            && currentTime.value() >= acceptedTime + flashHorizon - flashEarlyHideLead;
    }
    [[nodiscard]] bool hasObservedLiveProjectile() const noexcept { return hasHighestObserved; }
    [[nodiscard]] bool blocksHeldPrediction() const noexcept { return hasHighestObserved && !accepted; }

private:
    void resetSimulationRetryBackoff() noexcept
    {
        nextSimulationRetryDelayFrames = 1;
        hasRetryObservationSequence = false;
        hasScheduledRetry = false;
    }

    cs2::CEntityHandle localPawnHandle{};
    LiveGrenadeSnapshot acceptedSnapshot{};
    LiveGrenadeSnapshot newestObservedSnapshot{};
    float acceptedTime{};
    bool hasLocalPawnHandle{};
    bool accepted{};
    bool hasHighestObserved{};
    bool hasAcceptedTime{};
    bool hasRetryObservationSequence{};
    bool hasScheduledRetry{};
    std::uint32_t highestObservedObservationSequence{};
    std::uint32_t retryObservationSequence{};
    std::uint32_t nextSimulationRetryFrame{};
    std::uint32_t nextSimulationRetryDelayFrames{1};
};
