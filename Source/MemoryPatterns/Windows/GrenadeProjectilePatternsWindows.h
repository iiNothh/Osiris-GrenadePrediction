#pragma once

#include <MemoryPatterns/PatternTypes/GrenadeProjectilePatternTypes.h>
#include <MemorySearch/CodePattern.h>

struct GrenadeProjectilePatterns {
    [[nodiscard]] static consteval auto addClientPatterns(auto clientPatterns) noexcept
    {
        return clientPatterns
            .template addPattern<OffsetToGrenadeInitialPosition, CodePattern{"41 89 97 ? ? ? ? 4C 8D 86 ? ? ? ? 48 8D 96 ? ? ? ?"}.add(17).read()>()
            .template addPattern<OffsetToGrenadeInitialVelocity, CodePattern{"48 8D 8B ? ? ? ? E8 ? ? ? ? 33 FF 48 8D 8B ? ? ? ? 89 BB ? ? ? ?"}.add(3).read()>()
            .template addPattern<OffsetToGrenadeThrower, CodePattern{"8B 91 ? ? ? ? 33 FF 4C 8B 05 ? ? ? ? 4C 8B D1 83 FA FF 74 ?"}.add(2).read()>()
            .template addPattern<OffsetToExplodeEffectTickBegin, CodePattern{"33 FF 48 8D 8B ? ? ? ? 89 BB ? ? ? ? 48 89 BB ? ? ? ? 89 BB ? ? ? ?"}.add(24).read()>();
    }
};
