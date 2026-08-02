#include <gtest/gtest.h>

#include <CS2/Econ/ItemDefinitionIndex.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeKindMapper.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeSimulator.h>

namespace
{

struct UnusedTrace {
    [[nodiscard]] Optional<grenade_prediction::GrenadeTraceResult> traceGrenadeHull(cs2::Vector, cs2::Vector, void*) noexcept
    {
        return {};
    }
};

using Simulator = grenade_prediction::GrenadeSimulator<UnusedTrace>;

TEST(GrenadeKindMapperTest, MapsSupportedItemDefinitions)
{
    EXPECT_EQ(grenade_prediction::grenadeKindFromItemDefinition(cs2::ItemDefinitionIndex::Flashbang), grenade_prediction::GrenadeKind::Flashbang);
    EXPECT_EQ(grenade_prediction::grenadeKindFromItemDefinition(cs2::ItemDefinitionIndex::HEGrenade), grenade_prediction::GrenadeKind::HEGrenade);
    EXPECT_EQ(grenade_prediction::grenadeKindFromItemDefinition(cs2::ItemDefinitionIndex::SmokeGrenade), grenade_prediction::GrenadeKind::SmokeGrenade);
    EXPECT_EQ(grenade_prediction::grenadeKindFromItemDefinition(cs2::ItemDefinitionIndex::Molotov), grenade_prediction::GrenadeKind::Molotov);
    EXPECT_EQ(grenade_prediction::grenadeKindFromItemDefinition(cs2::ItemDefinitionIndex::Decoy), grenade_prediction::GrenadeKind::Decoy);
    EXPECT_EQ(grenade_prediction::grenadeKindFromItemDefinition(cs2::ItemDefinitionIndex::Incendiary), grenade_prediction::GrenadeKind::Incendiary);
    EXPECT_EQ(grenade_prediction::grenadeKindFromItemDefinition(cs2::ItemDefinitionIndex::M4A4), grenade_prediction::GrenadeKind::None);
}

TEST(GrenadeSimulatorPureTest, ClipsVelocityWithContactPushOff)
{
    const auto velocity = Simulator::clipVelocity({0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, 1.0f}, 2.0f, 0.03125f);
    EXPECT_FLOAT_EQ(velocity.z, 10.03125f);
}

TEST(GrenadeSimulatorPureTest, UsesGrenadeSpecificTerminationTimes)
{
    grenade_prediction::GrenadeSimulationParameters parameters{};
    EXPECT_FALSE(Simulator::shouldDetonate(parameters, grenade_prediction::GrenadeKind::HEGrenade, 104));
    EXPECT_TRUE(Simulator::shouldDetonate(parameters, grenade_prediction::GrenadeKind::HEGrenade, 105));
    EXPECT_FALSE(Simulator::shouldDetonate(parameters, grenade_prediction::GrenadeKind::Molotov, 136));
    EXPECT_TRUE(Simulator::shouldDetonate(parameters, grenade_prediction::GrenadeKind::Molotov, 137));
}

}
