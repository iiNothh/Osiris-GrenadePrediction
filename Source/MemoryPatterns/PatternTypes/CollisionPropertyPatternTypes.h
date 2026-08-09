#pragma once

#include <cstdint>

#include <CS2/Classes/CCollisionProperty.h>
#include <Utils/FieldOffset.h>
#include <Utils/StrongTypeAlias.h>

template <typename FieldType, typename OffsetType>
using CollisionPropertyOffset = FieldOffset<cs2::CCollisionProperty, FieldType, OffsetType>;

STRONG_TYPE_ALIAS(OffsetToCollisionMins, CollisionPropertyOffset<cs2::CCollisionProperty::m_vecMins, std::int8_t>);
STRONG_TYPE_ALIAS(OffsetToCollisionMaxs, CollisionPropertyOffset<cs2::CCollisionProperty::m_vecMaxs, std::int8_t>);
