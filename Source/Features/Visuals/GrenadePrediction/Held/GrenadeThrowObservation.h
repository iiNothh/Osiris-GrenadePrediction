#pragma once

#include <cstdint>

#include <Utils/Math.h>

enum class GrenadeThrowPhase : std::uint8_t {
    Observing,
    PendingExecution,
    Finalized
};

struct GrenadeThrowObservation {
    [[nodiscard]] bool observeWeapon(const void* weapon) noexcept
    {
        if (observedWeapon == weapon)
            return false;
        resetThrowSequence();
        observedWeapon = weapon;
        hasPinBaseline = false;
        previousPinPulled = false;
        return true;
    }

    [[nodiscard]] bool observePinState(const void* weapon, bool pinPulled) noexcept
    {
        if (observedWeapon != weapon) {
            static_cast<void>(observeWeapon(weapon));
            hasPinBaseline = true;
            previousPinPulled = pinPulled;
            return false;
        }
        const bool released = hasPinBaseline && previousPinPulled && !pinPulled;
        const bool startedNewPull = hasPinBaseline && !previousPinPulled && pinPulled;
        if (startedNewPull)
            resetThrowSequence();
        hasPinBaseline = true;
        previousPinPulled = pinPulled;
        return released;
    }

    void retainThrowStrength(float throwStrength) noexcept
    {
        if (!isStrengthLocked() && throwStrength >= 0.0f && throwStrength <= 1.0f) {
            retainedThrowStrength = throwStrength;
            hasRetainedThrowStrength = true;
        }
    }

    [[nodiscard]] bool observeThrowTime(const void* weapon, float throwTime) noexcept
    {
        if (weapon != observedWeapon)
            return false;
        if (!Math::isFinite(throwTime))
            return false;
        if (!(throwTime > 0.0f)) {
            if (phase != GrenadeThrowPhase::PendingExecution)
                return false;
            pendingThrowTime = 0.0f;
            phase = GrenadeThrowPhase::Observing;
            return true;
        }
        if (phase == GrenadeThrowPhase::Finalized)
            return false;
        if (phase != GrenadeThrowPhase::PendingExecution || pendingThrowTime != throwTime) {
            pendingThrowTime = throwTime;
            phase = GrenadeThrowPhase::PendingExecution;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool consumeActualExecution(bool hasCurtime, float curtime) noexcept
    {
        if (!hasCurtime || !Math::isFinite(curtime) || phase != GrenadeThrowPhase::PendingExecution || curtime <= pendingThrowTime)
            return false;
        phase = GrenadeThrowPhase::Finalized;
        return true;
    }

    [[nodiscard]] bool consumeLegacyRelease(bool releaseEdge) noexcept
    {
        if (!releaseEdge || phase != GrenadeThrowPhase::Observing)
            return false;
        phase = GrenadeThrowPhase::Finalized;
        return true;
    }

    [[nodiscard]] bool hasPendingExecution() const noexcept { return phase == GrenadeThrowPhase::PendingExecution; }
    [[nodiscard]] const void* pendingWeapon() const noexcept { return phase == GrenadeThrowPhase::PendingExecution ? observedWeapon : nullptr; }
    [[nodiscard]] std::uint32_t pendingSequence() const noexcept { return sequence; }
    [[nodiscard]] bool canCommitActualExecution() const noexcept { return hasRetainedThrowStrength; }
    [[nodiscard]] bool isStrengthLocked() const noexcept { return phase != GrenadeThrowPhase::Observing; }
    [[nodiscard]] bool isFinalized() const noexcept { return phase == GrenadeThrowPhase::Finalized; }

    void resetThrowSequence() noexcept
    {
        retainedThrowStrength = 1.0f;
        hasRetainedThrowStrength = false;
        pendingThrowTime = 0.0f;
        phase = GrenadeThrowPhase::Observing;
        ++sequence;
    }

    void reset() noexcept
    {
        observedWeapon = nullptr;
        hasPinBaseline = false;
        previousPinPulled = false;
        resetThrowSequence();
    }

    const void* observedWeapon{};
    bool hasPinBaseline{};
    bool previousPinPulled{};
    float retainedThrowStrength{1.0f};
    bool hasRetainedThrowStrength{};
    float pendingThrowTime{};
    GrenadeThrowPhase phase{GrenadeThrowPhase::Observing};
    std::uint32_t sequence{};
};
