#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <CS2/Classes/EntitySystem/CEntityHandle.h>
#include <CS2/Classes/Vector.h>
#include <CS2/Constants/EntityHandle.h>
#include <Features/Visuals/GrenadePrediction/GrenadeKind.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

struct LiveGrenadeSnapshot {
    cs2::CEntityHandle projectileHandle{};
    cs2::CEntityHandle throwerHandle{};
    cs2::Vector initialPosition{};
    cs2::Vector initialVelocity{};
    cs2::GrenadeKind kind{cs2::GrenadeKind::None};
    std::uint32_t observationSequence{};
    bool seen{};
    bool lifecycleEnded{};
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

    [[nodiscard]] LiveGrenadeCacheScanStatus status() const noexcept
    {
        return scanStatus;
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

    [[nodiscard]] static bool isValid(const LiveGrenadeSnapshot& grenade) noexcept
    {
        return isValidHandle(grenade.projectileHandle) && isValidHandle(grenade.throwerHandle) && grenade.kind != cs2::GrenadeKind::None
            && Math::isFinite(grenade.initialPosition.x) && Math::isFinite(grenade.initialPosition.y) && Math::isFinite(grenade.initialPosition.z)
            && Math::isFinite(grenade.initialVelocity.x) && Math::isFinite(grenade.initialVelocity.y) && Math::isFinite(grenade.initialVelocity.z);
    }

private:
    [[nodiscard]] static bool isValidHandle(cs2::CEntityHandle handle) noexcept
    {
        return handle.value != cs2::INVALID_EHANDLE_INDEX;
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
