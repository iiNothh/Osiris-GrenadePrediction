#pragma once

#include <MemoryPatterns/PatternTypes/DecoyProjectilePatternTypes.h>
#include <MemorySearch/CodePattern.h>

struct DecoyProjectilePatterns {
    [[nodiscard]] static consteval auto addClientPatterns(auto clientPatterns) noexcept
    {
        return clientPatterns
            .template addPattern<OffsetToDecoyShotTick, CodePattern{"8B 97 ? ? ? ? 3B 97 ? ? ? ? 0F 84 ? ? ? ?"}.add(2).read()>();
    }
};
