#pragma once

#include <MemoryPatterns/PatternTypes/EngineTracePatternTypes.h>
#include <MemorySearch/CodePattern.h>

struct EngineTracePatterns {
    [[nodiscard]] static consteval auto addClientPatterns(auto clientPatterns) noexcept
    {
        return clientPatterns
            .template addPattern<TraceShapeFunctionPointer, CodePattern{"0F 97 45 ? E8 ? ? ? ? 48 8D 55"}.add(5).abs()>()
            .template addPattern<BuildRnQueryShapeAttrFromAABBFunctionPointer, CodePattern{"F3 0F 10 42 0C 0F 2E 02 7A ? 75 ? F3 0F 10 42 10 0F 2E 42 04 7A ? 75 ? F3 0F 10 42 14 0F 2E 42 08"}>()
            .template addPattern<PhysicsWorldPointerSlotStoragePointer, CodePattern{"48 8B 0D ? ? ? ? 4C 8D 4B ? 4C 8D 43"}.add(3).abs()>()
            .template addPattern<CTraceFilterConstructionFunctionPointer, CodePattern{"48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 0F B6 41 ? 33 FF 24 C9"}>()
            .template addPattern<CTraceFilterAddExcludedEntityFunctionPointer, CodePattern{"E8 ? ? ? ? 48 8B 06 48 8D ? ? ? 48 8B CE FF 90 50 05 00 00"}.add(1).abs()>()
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
