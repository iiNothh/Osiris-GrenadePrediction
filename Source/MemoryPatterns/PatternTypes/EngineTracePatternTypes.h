#pragma once

#include <cstdint>

#include <CS2/Classes/Vector.h>
#include <Utils/StrongTypeAlias.h>

STRONG_TYPE_ALIAS(TraceShapeFunctionPointer, bool(*)(void*, const void*, const cs2::Vector*, const cs2::Vector*, void*, void*) noexcept);
STRONG_TYPE_ALIAS(GameTraceManagerStoragePointer, void**);
STRONG_TYPE_ALIAS(InitFilterFunctionPointer, void*(*)(void*, void*, std::uint64_t, std::uint8_t, std::uint8_t) noexcept);
STRONG_TYPE_ALIAS(AddSecondExcludedEntityToFilterFunctionPointer, void(*)(void*, void*, void*) noexcept);
STRONG_TYPE_ALIAS(CGameTraceEndPositionOffset, std::int32_t);
STRONG_TYPE_ALIAS(CGameTraceNormalOffset, std::int32_t);
STRONG_TYPE_ALIAS(CGameTraceFractionOffset, std::int32_t);
STRONG_TYPE_ALIAS(CGameTraceRawEntityHandleOffset, std::uint8_t);
