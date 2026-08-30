#pragma once

#include <cstddef>
#include <cstdint>

#include <CS2/Classes/Vector.h>
#include <Platform/Macros/IsPlatform.h>
#include <Utils/StrongTypeAlias.h>

STRONG_TYPE_ALIAS(TraceShapeFunctionPointer, bool(*)(void*, const void*, const cs2::Vector*, const cs2::Vector*, void*, void*) noexcept);
STRONG_TYPE_ALIAS(GameTraceManagerStoragePointer, void**);
STRONG_TYPE_ALIAS(InitFilterFunctionPointer, void*(*)(void*, void*, std::uint64_t, std::uint8_t, std::uint8_t) noexcept);
STRONG_TYPE_ALIAS(AddSecondExcludedEntityToFilterFunctionPointer, void(*)(void*, void*, void*) noexcept);
STRONG_TYPE_ALIAS(CGameTraceEndPositionOffset, std::int32_t);
STRONG_TYPE_ALIAS(CGameTraceNormalOffset, std::int32_t);
STRONG_TYPE_ALIAS(CGameTraceFractionOffset, std::int32_t);
STRONG_TYPE_ALIAS(CGameTraceRawEntityHandleOffset, std::uint8_t);

#if IS_WIN64()
STRONG_TYPE_ALIAS(CTraceFilterBaseVtablePointer, void**);
STRONG_TYPE_ALIAS(NativePipTraceFilterBodyProfilePointer, const std::byte*);
STRONG_TYPE_ALIAS(NativePipPushFilterConstructorPointer, const std::byte*);
STRONG_TYPE_ALIAS(PrimaryTraceShapeCandidateProfileMarker, const std::byte*);
STRONG_TYPE_ALIAS(SecondaryTraceShapeCandidateProfileMarker, const std::byte*);
STRONG_TYPE_ALIAS(PrimaryToSecondaryEnumerationCallsiteMarker, const std::byte*);
STRONG_TYPE_ALIAS(SecondaryCandidateEnumerationFunctionPointer, const std::byte*);
#endif
