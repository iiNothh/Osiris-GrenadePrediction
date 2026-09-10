#pragma once

#include <cstdint>

#include <CS2/Constants/InteractionLayers.h>
#include <CS2/EngineTrace/CGameTrace.h>
#include <CS2/EngineTrace/CTraceFilter.h>
#include <Utils/FieldOffset.h>
#include <Utils/StrongTypeAlias.h>

template <typename FieldType, typename OffsetType>
using CGameTraceOffset = FieldOffset<cs2::CGameTrace, FieldType, OffsetType>;

template <typename FieldType>
using CTraceFilterOffset = FieldOffset<cs2::CTraceFilter, FieldType, std::uint8_t>;

STRONG_TYPE_ALIAS(TraceShapeFunctionPointer, cs2::TraceShapeFunction);
STRONG_TYPE_ALIAS(BuildRnQueryShapeAttrFromAABBFunctionPointer, cs2::BuildRnQueryShapeAttrFromAABBFunction);
STRONG_TYPE_ALIAS(PhysicsWorldPointerSlotStoragePointer, cs2::PhysicsWorldPointerSlotStorage);
STRONG_TYPE_ALIAS(CTraceFilterConstructionFunctionPointer, cs2::CTraceFilterConstructionFunction);
STRONG_TYPE_ALIAS(CTraceFilterAddExcludedEntityFunctionPointer, cs2::CTraceFilterAddExcludedEntityFunction);
STRONG_TYPE_ALIAS(CGameTraceEndPositionOffset, CGameTraceOffset<cs2::Vector, std::int32_t>);
STRONG_TYPE_ALIAS(CGameTraceNormalOffset, CGameTraceOffset<cs2::Vector, std::int32_t>);
STRONG_TYPE_ALIAS(CGameTraceFractionOffset, CGameTraceOffset<float, std::int32_t>);
STRONG_TYPE_ALIAS(CGameTraceRawEntityHandleOffset, CGameTraceOffset<std::int32_t, std::uint8_t>);
STRONG_TYPE_ALIAS(CTraceFilterInteractsExcludeOffset, CTraceFilterOffset<cs2::engine_trace::InteractionLayer>);
STRONG_TYPE_ALIAS(CTraceFilterInteractsAsOffset, CTraceFilterOffset<cs2::engine_trace::InteractionLayer>);
STRONG_TYPE_ALIAS(CTraceFilterFlagsOffset, CTraceFilterOffset<std::uint8_t>);
STRONG_TYPE_ALIAS(CTraceFilterCandidateCollectionModeOffset, CTraceFilterOffset<std::uint8_t>);
