#pragma once

#include <MemoryPatterns/PatternTypes/BaseModelEntityPatternTypes.h>
#include <MemorySearch/CodePattern.h>

struct BaseModelEntityPatterns {
    [[nodiscard]] static consteval auto addClientPatterns(auto clientPatterns) noexcept
    {
        return clientPatterns
            .template addPattern<OffsetToGlowProperty, CodePattern{"90 ? ? ? ? ? 8B ? 48 8D 88 ? ? ? ?"}.add(11).read()>()
            .template addPattern<OffsetToCollisionProperty, CodePattern{"80 B9 ? ? ? ? ? 48 8D B9 ? ? ? ? 48 8B D9 74 ? 48 8B CF E8 ? ? ? ? 48 8B CF 48 8B D8 E8 ? ? ? ?"}.add(10).read()>();
    }
};
