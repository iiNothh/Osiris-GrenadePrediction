#include <gtest/gtest.h>

#include <GameClient/EngineTrace/EngineTrace.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeTraceAdapter.h>

namespace
{

struct Trace {
    Optional<TraceResult> result{};

    [[nodiscard]] Optional<TraceResult> traceGrenadeHull(cs2::Vector, cs2::Vector, void*) const noexcept
    {
        return result;
    }
};

TEST(GrenadeTraceAdapterTest, ConvertsEngineTraceResult)
{
    Trace trace{TraceResult{0.5f, {1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 1.0f}}};

    const auto result = grenade_prediction::GrenadeTraceAdapter{trace}.traceGrenadeHull({}, {}, nullptr);

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().fraction, 0.5f);
    EXPECT_EQ(result.value().endPosition, (cs2::Vector{1.0f, 2.0f, 3.0f}));
    EXPECT_EQ(result.value().normal, (cs2::Vector{0.0f, 0.0f, 1.0f}));
}

TEST(GrenadeTraceAdapterTest, PropagatesFailure)
{
    Trace trace{};

    EXPECT_FALSE(grenade_prediction::GrenadeTraceAdapter{trace}.traceGrenadeHull({}, {}, nullptr).hasValue());
}

}
