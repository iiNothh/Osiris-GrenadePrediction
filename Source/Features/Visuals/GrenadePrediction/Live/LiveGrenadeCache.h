#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <CS2/Classes/EntitySystem/CEntityHandle.h>
#include <CS2/Classes/Vector.h>
#include <CS2/Constants/EntityHandle.h>
#include <GameClient/Entities/GrenadeKind.h>
#include <GameClient/Entities/GrenadeProjectileSample.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

struct LiveGrenadeSnapshot {
    cs2::CEntityHandle projectileHandle{};
    cs2::CEntityHandle throwerHandle{};
    cs2::Vector initialPosition{};
    cs2::Vector initialVelocity{};
    GrenadeKind kind{GrenadeKind::None};
    std::uint32_t observationSequence{};
    bool seen{};
    bool lifecycleEnded{};
    Optional<GrenadeProjectileSample> currentSample{};
};

static_assert(std::is_trivially_copyable_v<LiveGrenadeSnapshot>);

enum class LiveGrenadeCacheScanStatus { Complete, Overflowed };

class LiveGrenadeCache {
public:
    static constexpr std::size_t maxEntries{64};

    void beginScan() noexcept
    {
        scanComplete = false;
        scanStatus = LiveGrenadeCacheScanStatus::Complete;
        for (std::size_t i = 0; i < grenadeCount; ++i)
            grenades[i].seen = false;
    }

    [[nodiscard]] bool upsert(LiveGrenadeSnapshot snapshot) noexcept
    {
        if (!isValid(snapshot))
            return false;

        if (!isFinite(snapshot.currentSample))
            snapshot.currentSample = {};

        for (std::size_t i = 0; i < grenadeCount; ++i) {
            if (grenades[i].projectileHandle == snapshot.projectileHandle) {
                snapshot.observationSequence = grenades[i].observationSequence;
                snapshot.seen = true;
                grenades[i] = snapshot;
                return true;
            }
        }

        if (grenadeCount == maxEntries) {
            scanStatus = LiveGrenadeCacheScanStatus::Overflowed;
            return false;
        }

        snapshot.observationSequence = nextObservationSequence();
        snapshot.seen = true;
        grenades[grenadeCount++] = snapshot;
        return true;
    }

    void endScan() noexcept
    {
        for (std::size_t i = 0; i < grenadeCount;) {
            if (!grenades[i].seen)
                grenades[i] = grenades[--grenadeCount];
            else
                ++i;
        }
        scanComplete = true;
    }

    void clear() noexcept
    {
        grenadeCount = 0;
        nextSequence = 1;
        scanComplete = true;
        scanStatus = LiveGrenadeCacheScanStatus::Complete;
    }

    void invalidate(cs2::CEntityHandle projectileHandle) noexcept
    {
        for (std::size_t i = 0; i < grenadeCount;) {
            if (grenades[i].projectileHandle == projectileHandle)
                grenades[i] = grenades[--grenadeCount];
            else
                ++i;
        }
    }

    [[nodiscard]] Optional<LiveGrenadeSnapshot> newestForThrower(cs2::CEntityHandle throwerHandle) const noexcept
    {
        if (!hasAuthoritativeScan() || !isValidHandle(throwerHandle))
            return {};

        Optional<LiveGrenadeSnapshot> newest;
        for (std::size_t i = 0; i < grenadeCount; ++i) {
            const auto& grenade = grenades[i];
            if (grenade.throwerHandle != throwerHandle || grenade.lifecycleEnded)
                continue;
            if (!newest.hasValue() || grenade.observationSequence > newest.value().observationSequence)
                newest = grenade;
        }
        return newest;
    }

    [[nodiscard]] Optional<LiveGrenadeSnapshot> find(cs2::CEntityHandle projectileHandle, cs2::CEntityHandle throwerHandle,
        std::uint32_t observationSequence) const noexcept
    {
        if (!hasAuthoritativeScan())
            return {};

        for (std::size_t i = 0; i < grenadeCount; ++i) {
            const auto& grenade = grenades[i];
            if (grenade.projectileHandle == projectileHandle && grenade.throwerHandle == throwerHandle
                && grenade.observationSequence == observationSequence)
                return grenade;
        }
        return {};
    }

    [[nodiscard]] bool contains(const LiveGrenadeSnapshot& snapshot) const noexcept
    {
        if (!hasAuthoritativeScan() || !isValid(snapshot))
            return false;

        for (std::size_t i = 0; i < grenadeCount; ++i) {
            const auto& grenade = grenades[i];
            if (grenade.projectileHandle == snapshot.projectileHandle && grenade.throwerHandle == snapshot.throwerHandle
                && grenade.observationSequence == snapshot.observationSequence)
                return !grenade.lifecycleEnded;
        }
        return false;
    }

    [[nodiscard]] bool isScanComplete() const noexcept
    {
        return scanComplete;
    }

    [[nodiscard]] bool hasOverflowed() const noexcept
    {
        return scanStatus == LiveGrenadeCacheScanStatus::Overflowed;
    }

    [[nodiscard]] bool hasAuthoritativeScan() const noexcept
    {
        return scanComplete && !hasOverflowed();
    }

private:
    [[nodiscard]] static bool isValid(const LiveGrenadeSnapshot& grenade) noexcept
    {
        return isValidHandle(grenade.projectileHandle) && isValidHandle(grenade.throwerHandle) && grenade.kind != GrenadeKind::None
            && Math::isFinite(grenade.initialPosition.x) && Math::isFinite(grenade.initialPosition.y) && Math::isFinite(grenade.initialPosition.z)
            && Math::isFinite(grenade.initialVelocity.x) && Math::isFinite(grenade.initialVelocity.y) && Math::isFinite(grenade.initialVelocity.z);
    }

    [[nodiscard]] static bool isValidHandle(cs2::CEntityHandle handle) noexcept
    {
        return handle.value != cs2::INVALID_EHANDLE_INDEX;
    }

    [[nodiscard]] static bool isFinite(const Optional<GrenadeProjectileSample>& sample) noexcept
    {
        return !sample.hasValue()
            || (Math::isFinite(sample.value().origin.x) && Math::isFinite(sample.value().origin.y) && Math::isFinite(sample.value().origin.z)
                && Math::isFinite(sample.value().velocity.x) && Math::isFinite(sample.value().velocity.y) && Math::isFinite(sample.value().velocity.z)
                && Math::isFinite(sample.value().worldTime) && Math::isFinite(sample.value().createTime) && Math::isFinite(sample.value().worldTickInterval));
    }

    LiveGrenadeSnapshot grenades[maxEntries]{};
    std::size_t grenadeCount{};
    [[nodiscard]] std::uint32_t nextObservationSequence() noexcept
    {
        const auto sequence = nextSequence;
        if (++nextSequence == 0)
            nextSequence = 1;
        return sequence;
    }

    std::uint32_t nextSequence{1};
    bool scanComplete{true};
    LiveGrenadeCacheScanStatus scanStatus{LiveGrenadeCacheScanStatus::Complete};
};
