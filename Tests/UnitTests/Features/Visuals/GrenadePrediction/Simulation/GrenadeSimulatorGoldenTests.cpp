#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeSimulator.h>

namespace
{

struct FreeFlightTrace {
    std::size_t calls{};

    [[nodiscard]] Optional<grenade_prediction::GrenadeTraceResult> traceGrenadeHull(cs2::Vector, cs2::Vector end, void*) noexcept
    {
        ++calls;
        return grenade_prediction::GrenadeTraceResult{1.0f, end, {}};
    }
};

class GrenadeSimulatorGoldenTest : public testing::Test {
protected:
    FreeFlightTrace trace{};
    grenade_prediction::GrenadeSimulator<FreeFlightTrace> simulator{trace};
    grenade_prediction::GrenadeSimulationResult result{};
};

TEST_F(GrenadeSimulatorGoldenTest, FreeFlightMatchesPaddedHeTracerHorizon)
{
    simulator.simulate({}, {.initialVelocity = {100.0f, 0.0f, 0.0f}, .kind = grenade_prediction::GrenadeKind::HEGrenade}, result);
    ASSERT_TRUE(result.trajectory.valid);
    EXPECT_EQ(trace.calls, 210U);
    EXPECT_EQ(result.trajectory.pointCount, 54U);
    EXPECT_NEAR(result.trajectory.points[1].x, 3.125f, 0.0001f);
    EXPECT_NEAR(result.trajectory.points[1].z, -0.15625f, 0.0001f);
    EXPECT_NEAR(result.trajectory.endPosition.x, 164.0625f, 0.0001f);
    EXPECT_NEAR(result.trajectory.endPosition.z, -430.6640625f, 0.0001f);
}

TEST_F(GrenadeSimulatorGoldenTest, MidairMolotovTimeoutSuppressesLandingMarker)
{
    simulator.simulate({}, {.kind = grenade_prediction::GrenadeKind::Molotov}, result);
    ASSERT_TRUE(result.trajectory.valid);
    EXPECT_FALSE(result.trajectory.validLanding);
    EXPECT_EQ(trace.calls, 274U);
    EXPECT_NEAR(result.trajectory.endPosition.z, -733.1640625f, 0.0001f);
}

}
