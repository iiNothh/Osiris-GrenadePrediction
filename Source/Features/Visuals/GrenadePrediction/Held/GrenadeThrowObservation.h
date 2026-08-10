#pragma once

#include <cstdint>

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
        if (!(throwTime > 0.0f)) {
            if (!hasPendingThrowTime)
                return false;
            pendingThrowTime = 0.0f;
            hasPendingThrowTime = false;
            return true;
        }
        if (finalized)
            return false;
        if (!hasPendingThrowTime || pendingThrowTime != throwTime) {
            pendingThrowTime = throwTime;
            hasPendingThrowTime = true;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool consumeActualExecution(bool hasCurtime, float curtime) noexcept
    {
        if (!hasCurtime || !hasPendingThrowTime || finalized || curtime <= pendingThrowTime)
            return false;
        finalized = true;
        hasPendingThrowTime = false;
        return true;
    }

    [[nodiscard]] bool consumeLegacyRelease(bool releaseEdge) noexcept
    {
        if (!releaseEdge || hasPendingThrowTime || finalized)
            return false;
        finalized = true;
        return true;
    }

    [[nodiscard]] bool hasPendingExecution() const noexcept { return hasPendingThrowTime; }
    [[nodiscard]] const void* pendingWeapon() const noexcept { return hasPendingThrowTime ? observedWeapon : nullptr; }
    [[nodiscard]] std::uint32_t pendingSequence() const noexcept { return sequence; }
    [[nodiscard]] bool canCommitActualExecution() const noexcept { return hasRetainedThrowStrength; }
    [[nodiscard]] bool isStrengthLocked() const noexcept { return hasPendingThrowTime || finalized; }
    [[nodiscard]] bool isFinalized() const noexcept { return finalized; }

    void resetThrowSequence() noexcept
    {
        retainedThrowStrength = 1.0f;
        hasRetainedThrowStrength = false;
        pendingThrowTime = 0.0f;
        hasPendingThrowTime = false;
        finalized = false;
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
    bool hasPendingThrowTime{};
    bool finalized{};
    std::uint32_t sequence{};
};
