#pragma once

#include <cstdint>

#include <CS2/Classes/EntitySystem/CEntityHandle.h>
#include <CS2/Constants/EntityHandle.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

enum class GrenadeThrowPhase : std::uint8_t {
    Observing,
    PendingExecution,
    Finalized
};

struct GrenadeThrowObservation {
    [[nodiscard]] bool observeWeapon(cs2::CEntityHandle weapon) noexcept
    {
        if (observedWeapon == weapon)
            return false;
        startNewThrowSequence();
        observedWeapon = weapon;
        hasPinBaseline = false;
        previousPinPulled = false;
        return true;
    }

    [[nodiscard]] bool observePinState(cs2::CEntityHandle weapon, bool pinPulled) noexcept
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
            startNewThrowSequence();
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

    [[nodiscard]] bool observeThrowTime(cs2::CEntityHandle weapon, float throwTime) noexcept
    {
        if (weapon != observedWeapon)
            return false;
        if (!Math::isFinite(throwTime))
            return false;
        if (!(throwTime > 0.0f)) {
            if (phase == GrenadeThrowPhase::Finalized) {
                startNewThrowSequence();
                return true;
            }
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
    [[nodiscard]] cs2::CEntityHandle pendingWeapon() const noexcept
    {
        return phase == GrenadeThrowPhase::PendingExecution ? observedWeapon : cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX};
    }

    template <typename ReadThrowStrength>
    void captureThrowStrength(bool pinPulled, Optional<float> throwTime, ReadThrowStrength&& readThrowStrength) noexcept
    {
        const bool hasPositiveThrowTime = throwTime.hasValue() && Math::isFinite(throwTime.value()) && throwTime.value() > 0.0f;
        if (isStrengthLocked() || (!pinPulled && !hasPositiveThrowTime))
            return;
        const auto throwStrength = readThrowStrength();
        if (throwStrength.hasValue())
            retainThrowStrength(throwStrength.value());
    }
    [[nodiscard]] std::uint32_t pendingSequence() const noexcept { return sequence; }
    [[nodiscard]] bool canCommitTrajectory() const noexcept { return hasRetainedThrowStrength; }
    [[nodiscard]] bool isStrengthLocked() const noexcept { return phase != GrenadeThrowPhase::Observing; }
    [[nodiscard]] bool isFinalized() const noexcept { return phase == GrenadeThrowPhase::Finalized; }

    void startNewThrowSequence() noexcept
    {
        retainedThrowStrength = 1.0f;
        hasRetainedThrowStrength = false;
        pendingThrowTime = 0.0f;
        phase = GrenadeThrowPhase::Observing;
        ++sequence;
    }

    void reset() noexcept
    {
        observedWeapon = cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX};
        hasPinBaseline = false;
        previousPinPulled = false;
        startNewThrowSequence();
    }

    cs2::CEntityHandle observedWeapon{cs2::INVALID_EHANDLE_INDEX};
    bool hasPinBaseline{};
    bool previousPinPulled{};
    float retainedThrowStrength{1.0f};
    bool hasRetainedThrowStrength{};
    float pendingThrowTime{};
    GrenadeThrowPhase phase{GrenadeThrowPhase::Observing};
    std::uint32_t sequence{};
};
