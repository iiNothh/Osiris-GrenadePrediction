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
            .template addPattern<CGameTraceEndPositionOffset, CodePattern{"F2 0F 11 83 ? ? ? ? F3 0F 11 93 ? ? ? ? 48 8B 9C 24 ? ? ? ?"}.add(4).read()>()
            .template addPattern<CGameTraceNormalOffset, CodePattern{"F2 41 0F 10 46 1C F2 0F 11 83 ? ? ? ? 41 8B 46 24 89 83 ? ? ? ?"}.add(10).read()>()
            .template addPattern<CGameTraceFractionOffset, CodePattern{"41 8B 46 34 89 83 ? ? ? ? 41 0F B6 46 40 88 83 ? ? ? ?"}.add(6).read()>()
            .template addPattern<CGameTraceRawEntityHandleOffset, CodePattern{"0F 11 ? ? F2 ? 0F 10 ? ? F2 0F 11 ? ? 89 6B ?"}.add(17).read()>()
            .template addPattern<CTraceFilterBaseVtablePointer, CodePattern{"48 8D 05 ? ? ? ? 48 89 79 ? 48 89 01 48 8B F2"}.add(3).abs()>()
            .template addPattern<NativePipTraceFilterBodyProfilePointer, CodePattern{"48 B9 01 30 00 00 02 00 00 00 C7 44 24 ? 00 00 00 C0 48 89 4C 24 ? 4C 8D 4C 24 ? 48 B8 01 00 00 00 80 00 00 00 C7 44 24 ? 00 00 00 C0 48 89 44 24 ? 41 B8 10 00 00 00 48 8D 44 24 ? C7 44 24 ? 00 00 00 C0 B2 0A 48 89 44 24 ? 49 8B CF C7 44 24 ? 00 00 00 40 C7 44 24 ? 00 00 00 40 C7 44 24 ? 00 00 00 40 E8 ? ? ? ? BA 00 02 04 00 49 8B CF E8 ? ? ? ?"}.add(105).abs()>()
            .template addPattern<NativePipPushFilterConstructorPointer, CodePattern{"E8 ? ? ? ? 48 8B 07 48 8B CF FF 90 ? ? ? ? 48 85 C0 74 ? 48 8B 07 48 8B CF FF 90 ? ? ? ? F6 40 ? 03 74 ? 80 78 ? 00 75 ? 48 81 64 24 ? FF FF F7 FF"}.add(1).abs()>()
            .template addPattern<PrimaryTraceShapeCandidateProfileMarker, CodePattern{"49 8B CD 48 89 44 24 ? 4D 8B CC 4C 8B C6 E8 ? ? ? ? 49 83 C6 48 48 81 C3 C0 00 00 00 49 83 EF 01 75 ? 48 8B B5 ? ? ? ? 8B 16 48 8B 8D ? ? ? ? 80 79 40 00 0F 84 ? ? ? ? 45 33 ED 45 33 E4 85 D2 0F 8E ? ? ? ? 45 33 C0 45 33 FF 45 33 F6 4C 89 85 ? ? ? ? 33 DB ? ? ? ? ? ? ? ? ? ? ? 48 8B 07 81 7C 03 68 00 80 00 00 74 ? 48 8B 54 03 08 48 85 D2 0F 84 ? ? ? ? 48 8B 01 FF 50 08 4C 8B 85 ? ? ? ? 84 C0 0F 84 ? ? ? ?"}>()
            .template addPattern<SecondaryTraceShapeCandidateProfileMarker, CodePattern{"41 80 7C 24 40 00 74 ? 49 8B 04 24 49 8B D6 49 8B CC FF 50 08 84 C0 0F 84 ? ? ? ?"}>()
            .template addPattern<PrimaryToSecondaryEnumerationCallsiteMarker, CodePattern{"0F B6 41 08 D0 E8 A8 01 74 ? 48 8B 95 ? ? ? ? 4C 8D 4C 24 ? 48 89 74 24 ? 4D 8B C4 C7 44 24 ? 01 00 00 00 C7 44 24 ? 00 00 00 00 48 89 4C 24 ? 48 8B 0D ? ? ? ? E8 ? ? ? ?"}>()
            .template addPattern<SecondaryCandidateEnumerationFunctionPointer, CodePattern{"0F B6 41 08 D0 E8 A8 01 74 ? 48 8B 95 ? ? ? ? 4C 8D 4C 24 ? 48 89 74 24 ? 4D 8B C4 C7 44 24 ? 01 00 00 00 C7 44 24 ? 00 00 00 00 48 89 4C 24 ? 48 8B 0D ? ? ? ? E8 ? ? ? ?"}.add(59).abs()>();
    }
};
