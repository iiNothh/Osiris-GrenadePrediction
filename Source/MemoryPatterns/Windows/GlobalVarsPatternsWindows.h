#pragma once

#include <MemoryPatterns/PatternTypes/GlobalVarsPatternTypes.h>
#include <MemorySearch/CodePattern.h>

struct GlobalVarsPatterns {
    [[nodiscard]] static consteval auto addClientPatterns(auto clientPatterns) noexcept
    {
        return clientPatterns
            .template addPattern<OffsetToFrametime, CodePattern{"0F 10 ? ? 39 ? ? ? ? ? 75 ? 48"}.add(3).read()>()
            .template addPattern<GlobalVarsTickCountOffset, CodePattern{"48 8B 0D ? ? ? ? F3 0F 10 71 ? 8B 59 ? 0F 5A F6"}.add(14).read()>()
            .template addPattern<GlobalVarsTickIntervalFunction, CodePattern{"48 8B 0D ? ? ? ? E8 ? ? ? ? 8B 45 ? 2B 43 ? 66 0F 6E C8 0F 5B C9 F3 0F 59 C1"}.add(8).abs()>();
    }
};
