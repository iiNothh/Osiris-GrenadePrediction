#pragma once

#include <MemoryPatterns/PatternTypes/EngineTracePatternTypes.h>
#include <MemorySearch/CodePattern.h>

struct EngineTracePatterns {
    [[nodiscard]] static consteval auto addClientPatterns(auto clientPatterns) noexcept
    {
        return clientPatterns
            .template addPattern<TraceShapeFunctionPointer, CodePattern{"0F 97 45 ? E8 ? ? ? ? 48 8D 55"}.add(5).abs()>()
            .template addPattern<GameTraceManagerStoragePointer, CodePattern{"48 8B 0D ? ? ? ? 4C 8D 4B ? 4C 8D 43"}.add(3).abs()>()
            .template addPattern<InitFilterFunctionPointer, CodePattern{"48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 0F B6 41 ? 33 FF 24 C9"}>()
            .template addPattern<AddSecondExcludedEntityToFilterFunctionPointer, CodePattern{"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B E9 49 8B F0 48 8B CA 48 8B FA E8 ? ? ? ? 48 8B CE 89 45 20 E8 ? ? ? ? 48 8B CF 89 45 24"}>()
            .template addPattern<CTraceFilterInteractsExcludeOffset, CodePattern{"48 89 79 ? 48 89 01 48 8B F2 0F B6 44 24 ? 88 41 ? 48 89 79 ?"}.add(3).read()>()
            .template addPattern<CTraceFilterInteractsAsOffset, CodePattern{"49 8B 46 ? 4C 8B CF 48 8B 9D ? ? ? ?"}.add(3).read()>()
            .template addPattern<CTraceFilterFlagsOffset, CodePattern{"48 8D 05 ? ? ? ? 80 4B ? 02 48 89 03"}.add(9).read()>()
            .template addPattern<CTraceFilterCandidateCollectionModeOffset, CodePattern{"41 38 5C 24 ? 0F 85 ? ? ? ? 48 8B 0E 48 8D 54 24 ? 0F 57 C0"}.add(4).read()>()
            .template addPattern<CGameTraceEndPositionOffset, CodePattern{"F2 0F 11 83 ? ? ? ? F3 0F 11 93 ? ? ? ? 48 8B 9C 24 ? ? ? ?"}.add(4).read()>()
            .template addPattern<CGameTraceNormalOffset, CodePattern{"F2 41 0F 10 46 1C F2 0F 11 83 ? ? ? ? 41 8B 46 24 89 83 ? ? ? ?"}.add(10).read()>()
            .template addPattern<CGameTraceFractionOffset, CodePattern{"41 8B 46 34 89 83 ? ? ? ? 41 0F B6 46 40 88 83 ? ? ? ?"}.add(6).read()>()
            .template addPattern<CGameTraceRawEntityHandleOffset, CodePattern{"0F 11 ? ? F2 ? 0F 10 ? ? F2 0F 11 ? ? 89 6B ?"}.add(17).read()>();
    }
};
