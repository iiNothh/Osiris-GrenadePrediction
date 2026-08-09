#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <CS2/Classes/EntitySystem/CEntityHandle.h>
#include <CS2/Classes/Vector.h>
#include <CS2/Constants/EntityHandle.h>
#include <Features/Visuals/GrenadePrediction/GrenadeKind.h>
#include <Utils/DynamicArray.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

struct LiveGrenadeSnapshot {
    cs2::CEntityHandle projectileHandle{};
    cs2::CEntityHandle throwerHandle{};
    cs2::Vector initialPosition{};
    cs2::Vector initialVelocity{};
    cs2::GrenadeKind kind{cs2::GrenadeKind::None};
    std::uint64_t firstObservationSequence{};
    bool seen{};
};

static_assert(std::is_trivially_copyable_v<LiveGrenadeSnapshot>);

class LiveGrenadeCache {
public:
    static constexpr std::size_t maxEntries{64};

    void beginScan() noexcept
    {
        scanComplete = false;
        for (auto& grenade : grenades)
            grenade.seen = false;
    }

    [[nodiscard]] bool upsert(LiveGrenadeSnapshot snapshot) noexcept
    {
        if (!isValid(snapshot))
            return false;

        for (auto& grenade : grenades) {
            if (grenade.projectileHandle == snapshot.projectileHandle) {
                snapshot.firstObservationSequence = grenade.firstObservationSequence;
                snapshot.seen = true;
                grenade = snapshot;
                return true;
            }
        }

        if (grenades.getSize() == maxEntries || nextFirstObservationSequence == std::numeric_limits<std::uint64_t>::max())
            return false;

        snapshot.firstObservationSequence = nextFirstObservationSequence;
        snapshot.seen = true;
        if (!grenades.pushBack(snapshot))
            return false;
        ++nextFirstObservationSequence;
        return true;
    }

    void endScan() noexcept
    {
        for (std::size_t i = 0; i < grenades.getSize();) {
            if (!grenades[i].seen)
                grenades.fastRemoveAt(i);
            else
                ++i;
        }
        scanComplete = true;
    }

    void clear() noexcept
    {
        grenades.clear();
        nextFirstObservationSequence = 1;
        scanComplete = true;
    }

    [[nodiscard]] Optional<LiveGrenadeSnapshot> newestForThrower(cs2::CEntityHandle throwerHandle) const noexcept
    {
        if (!scanComplete || !isValidHandle(throwerHandle))
            return {};

        Optional<LiveGrenadeSnapshot> newest;
        for (const auto& grenade : grenades) {
            if (grenade.throwerHandle != throwerHandle)
                continue;
            if (!newest.hasValue() || grenade.firstObservationSequence > newest.value().firstObservationSequence)
                newest = grenade;
        }
        return newest;
    }

    [[nodiscard]] const DynamicArray<LiveGrenadeSnapshot>& entries() const noexcept
    {
        return grenades;
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

    DynamicArray<LiveGrenadeSnapshot> grenades;
    std::uint64_t nextFirstObservationSequence{1};
    bool scanComplete{true};
};
