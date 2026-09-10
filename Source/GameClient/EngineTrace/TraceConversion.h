#pragma once

#include <cstdint>

#include <CS2/Classes/Vector.h>
#include <CS2/EngineTrace/AABB_t.h>
#include <CS2/EngineTrace/CGameTrace.h>
#include <CS2/EngineTrace/RnQueryShapeAttr_t.h>
#include <GameClient/EngineTrace/HullTraceRequest.h>
#include <GameClient/EngineTrace/TraceLayout.h>
#include <GameClient/EngineTrace/TraceResult.h>
#include <Utils/Optional.h>

namespace engine_trace {
    [[nodiscard]] inline cs2::RnQueryShapeAttr_t makeQueryShape(cs2::BuildRnQueryShapeAttrFromAABBFunction buildQueryShape, const HullTraceRequest &request) noexcept
    {
        cs2::RnQueryShapeAttr_t queryShape{};
        const cs2::AABB_t bounds{
            .m_vMinBounds = request.mins,
            .m_vMaxBounds = request.maxs
        };
        buildQueryShape(&queryShape, &bounds);
        return queryShape;
    }

    [[nodiscard]] inline Optional<TraceResult> decodeTraceOutput(const cs2::CGameTrace &output, const TraceOutputLayout &layout) noexcept
    {
        if (!hasValidTraceOutputLayout(layout))
            return {};

        TraceResult result{
            .fraction = output.readValue<float>(layout.fractionOffset),
            .endPos = output.readValue<cs2::Vector>(layout.endPositionOffset),
            .normal = output.readValue<cs2::Vector>(layout.normalOffset)
        };
        if (!result.isValid())
            return {};

        if (result.fraction < 1.0f && layout.hasValidRawEntityHandleOffset())
            result.rawEntityHandle = output.readValue<std::int32_t>(layout.rawEntityHandleOffset.value());
        return result;
    }
}
