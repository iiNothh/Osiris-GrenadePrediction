#pragma once

#include <MemoryPatterns/PatternTypes/CollisionPropertyPatternTypes.h>
#include <MemorySearch/CodePattern.h>

struct CollisionPropertyPatterns {
    [[nodiscard]] static consteval auto addClientPatterns(auto clientPatterns) noexcept
    {
        return clientPatterns
            .template addPattern<OffsetToCollisionMins, CodePattern{"F2 0F 10 59 ? 48 8B C2 F3 0F 10 51 ? 0F 28 CB F3 0F 10 41 ? 0F C6 C9 ? F3 0F 5C C3 F3 0F 5C D1 F3 0F 10 49 ? F3 0F 5C 49 ?"}.add(4).read()>()
            .template addPattern<OffsetToCollisionMaxs, CodePattern{"F2 0F 10 59 ? 48 8B C2 F3 0F 10 51 ? 0F 28 CB F3 0F 10 41 ? 0F C6 C9 ? F3 0F 5C C3 F3 0F 5C D1 F3 0F 10 49 ? F3 0F 5C 49 ?"}.add(20).read()>();
    }
};
