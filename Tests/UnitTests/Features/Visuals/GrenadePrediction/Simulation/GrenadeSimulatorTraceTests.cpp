#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeSimulator.h>

namespace
{

struct ScriptedGrenadeTrace {
    grenade_prediction::GrenadeTraceResult results[8]{};
    bool failures[8]{};
    std::size_t resultCount{};
    std::size_t calls{};

    void add(grenade_prediction::GrenadeTraceResult result) noexcept
    {
        results[resultCount++] = result;
    }

    void addFailure() noexcept
    {
        failures[resultCount++] = true;
    }

    [[nodiscard]] Optional<grenade_prediction::GrenadeTraceResult> traceGrenadeHull(cs2::Vector, cs2::Vector, void*) noexcept
    {
        const std::size_t index = calls++;
        if (index >= resultCount || failures[index])
            return {};
        return Optional<grenade_prediction::GrenadeTraceResult>{results[index]};
    }
};

class GrenadeSimulatorTraceTest : public testing::Test {
protected:
    ScriptedGrenadeTrace trace{};
    grenade_prediction::GrenadeSimulator<ScriptedGrenadeTrace> simulator{trace};
    grenade_prediction::GrenadeSimulationResult result{};
};

TEST_F(GrenadeSimulatorTraceTest, InvalidatesTheTrajectoryWhenTracingFails)
{
    trace.addFailure();
    simulator.simulate({}, {.initialVelocity = {64.0f, 0.0f, 0.0f}, .kind = grenade_prediction::GrenadeKind::HEGrenade}, result);
    EXPECT_TRUE(result.traceFailed);
    EXPECT_FALSE(result.trajectory.valid);
    EXPECT_EQ(result.trajectory.pointCount, 0U);
}

TEST_F(GrenadeSimulatorTraceTest, RecordsPrimaryBounceAndUsesContinuationTrace)
{
    trace.add({0.5f, {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}});
    trace.add({1.0f, {}, {}});
    trace.add({1.0f, {}, {}});
    simulator.simulate({.maximumTicks = 1}, {.initialVelocity = {64.0f, 0.0f, 0.0f}, .kind = grenade_prediction::GrenadeKind::HEGrenade}, result);
    EXPECT_EQ(trace.calls, 3U);
    ASSERT_EQ(result.trajectory.bounceCount, 1U);
    EXPECT_EQ(result.trajectory.bounces[0], (cs2::Vector{1.0f, 0.0f, 0.0f}));
    EXPECT_FALSE(result.traceFailed);
}

TEST_F(GrenadeSimulatorTraceTest, DetonatesMolotovOnFlatContact)
{
    trace.add({0.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}});
    simulator.simulate({}, {.initialVelocity = {0.0f, 0.0f, -64.0f}, .kind = grenade_prediction::GrenadeKind::Molotov}, result);
    EXPECT_TRUE(result.trajectory.valid);
    EXPECT_TRUE(result.trajectory.validLanding);
    EXPECT_TRUE(result.detonated);
    EXPECT_EQ(trace.calls, 1U);
}

}
