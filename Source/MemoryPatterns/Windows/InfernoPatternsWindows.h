#pragma once

#include <MemoryPatterns/PatternTypes/InfernoPatternTypes.h>
#include <MemorySearch/CodePattern.h>

struct InfernoPatterns {
    [[nodiscard]] static consteval auto addClientPatterns(auto clientPatterns) noexcept
    {
        return clientPatterns
            .template addPattern<OffsetToFirePositions, CodePattern{"C1 E0 08 33 D0 8D 41 BF 3C 19 77 ? 80 C1 20 0F B6 C1 4C 8D 0D ? ? ? ? 33 C2 C7 44 24 ? ? ? ? ? 69 C8 95 E9 D1 5B 4C 8D 44 24 ? 48 8D 15 ? ? ? ? C7 44 24 ? 00 03 00 00"}.add(31).read()>()
            .template addPattern<OffsetToFireIsBurning, CodePattern{"4C 8D 44 24 ? C7 44 24 ? ? ? ? ? C7 44 24 ? 40 00 00 00"}.add(9).read()>()
            .template addPattern<OffsetToFireCount, CodePattern{"48 89 AB ? ? ? ? E8 ? ? ? ? 40 88 AB ? ? ? ?"}.add(3).read()>()
            .template addPattern<OffsetToFireLifetime, CodePattern{"48 8D 8B ? ? ? ? 49 89 6E ? 48 8D 05 ? ? ? ? 48 89 83 ? ? ? ?"}.add(3).read()>();
    }
};
