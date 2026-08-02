#pragma once

#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeSimulator.h>
#include <Utils/Optional.h>

namespace grenade_prediction
{

template <typename Trace>
class GrenadeTraceAdapter {
public:
    explicit GrenadeTraceAdapter(Trace& trace) noexcept
        : trace{trace}
    {
    }

    [[nodiscard]] Optional<GrenadeTraceResult> traceGrenadeHull(cs2::Vector start, cs2::Vector end, void* ignoredEntity) const noexcept
    {
        const auto traceResult = trace.traceGrenadeHull(start, end, ignoredEntity);
        if (!traceResult.hasValue())
            return {};
        return GrenadeTraceResult{traceResult.value().fraction, traceResult.value().endPos, traceResult.value().normal};
    }

private:
    Trace& trace;
};

}
