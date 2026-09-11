#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadeLaunchSelection.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeLaunchFallback.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeThrowObservation.h>
#include <Mocks/GrenadePrediction/ScriptedGrenadeTrace.h>

namespace
{

using Simulator = GrenadeSimulator<GrenadeSimulatorTestHookContext>;

struct ScriptedFallbackBaseEntity {
    Optional<cs2::Vector> viewOffsetValue;
    Optional<cs2::Vector> velocity;
    int viewOffsetCalls{};
    int velocityCalls{};
    int* callOrder{};
    int velocityCallOrder{};

    [[nodiscard]] Optional<cs2::Vector> viewOffset() noexcept
    {
        ++viewOffsetCalls;
        return viewOffsetValue;
    }

    [[nodiscard]] Optional<cs2::Vector> absVelocity() noexcept
    {
        ++velocityCalls;
        if (callOrder)
            velocityCallOrder = ++*callOrder;
        return velocity;
    }
};

struct ScriptedFallbackPlayerPawn {
    Optional<cs2::Vector> angles;
    Optional<cs2::Vector> origin;
    ScriptedFallbackBaseEntity entity;

    [[nodiscard]] Optional<cs2::Vector> eyeAngles() noexcept { return angles; }
    [[nodiscard]] Optional<cs2::Vector> absOrigin() noexcept { return origin; }
    [[nodiscard]] ScriptedFallbackBaseEntity& baseEntity() noexcept { return entity; }
};

struct ScriptedFallbackSimulator {
    Optional<cs2::Vector> spawnResult;
    cs2::Vector initialVelocity;
    int spawnCalls{};
    int initialVelocityCalls{};
    int* callOrder{};
    int initialVelocityCallOrder{};
    cs2::Vector spawnEyePosition;
    cs2::Vector spawnAngles;
    float spawnStrength{};
    void* spawnSkipEntity{};
    cs2::Vector velocityAngles;
    float baseVelocity{};
    float velocityStrength{};

    [[nodiscard]] Optional<cs2::Vector> computeSpawnPosition(cs2::Vector eyePosition, cs2::Vector angles, float throwStrength, void* skipEntity) noexcept
    {
        ++spawnCalls;
        spawnEyePosition = eyePosition;
        spawnAngles = angles;
        spawnStrength = throwStrength;
        spawnSkipEntity = skipEntity;
        return spawnResult;
    }

    [[nodiscard]] cs2::Vector computeInitialVelocity(cs2::Vector angles, float velocity, float throwStrength) noexcept
    {
        ++initialVelocityCalls;
        if (callOrder)
            initialVelocityCallOrder = ++*callOrder;
        velocityAngles = angles;
        baseVelocity = velocity;
        velocityStrength = throwStrength;
        return initialVelocity;
    }
};

[[nodiscard]] ScriptedFallbackPlayerPawn makeFallbackPlayer() noexcept
{
    return {.angles = Optional<cs2::Vector>{cs2::Vector{10.0f, 20.0f, 30.0f}}, .origin = Optional<cs2::Vector>{cs2::Vector{100.0f, 200.0f, 300.0f}}};
}

TEST(ManualGrenadeLaunchTest, FallbackReturnsEmptyForMissingEyeAnglesOrOrigin)
{
    auto player = makeFallbackPlayer();
    ScriptedFallbackSimulator simulator;
    player.angles = {};

    EXPECT_FALSE(computeHeldGrenadeLaunchFallback(player, simulator, 0.5f, nullptr).hasValue());
    EXPECT_EQ(simulator.spawnCalls, 0);

    player = makeFallbackPlayer();
    player.origin = {};
    EXPECT_FALSE(computeHeldGrenadeLaunchFallback(player, simulator, 0.5f, nullptr).hasValue());
    EXPECT_EQ(simulator.spawnCalls, 0);
}

TEST(ManualGrenadeLaunchTest, FallbackUsesInteriorViewOffsetAndDefaultForInvalidOffsets)
{
    const auto expectEyeHeight = [](Optional<cs2::Vector> viewOffset, float expectedEyeHeight) noexcept {
        auto player = makeFallbackPlayer();
        player.entity.viewOffsetValue = viewOffset;
        ScriptedFallbackSimulator simulator{.spawnResult = cs2::Vector{}};

        ASSERT_TRUE(computeHeldGrenadeLaunchFallback(player, simulator, 0.5f, nullptr).hasValue());
        EXPECT_FLOAT_EQ(simulator.spawnEyePosition.z, 300.0f + expectedEyeHeight);
    };

    expectEyeHeight(Optional<cs2::Vector>{cs2::Vector{0.0f, 0.0f, 55.0f}}, 55.0f);
    expectEyeHeight({}, 64.06f);
    expectEyeHeight(Optional<cs2::Vector>{cs2::Vector{0.0f, 0.0f, 30.0f}}, 64.06f);
    expectEyeHeight(Optional<cs2::Vector>{cs2::Vector{0.0f, 0.0f, 70.0f}}, 64.06f);
}

TEST(ManualGrenadeLaunchTest, FallbackSkipsVelocityLookupWhenSpawnFails)
{
    auto player = makeFallbackPlayer();
    player.entity.velocity = cs2::Vector{1.0f, 2.0f, 3.0f};
    ScriptedFallbackSimulator simulator;

    EXPECT_FALSE(computeHeldGrenadeLaunchFallback(player, simulator, 0.5f, nullptr).hasValue());
    EXPECT_EQ(player.entity.viewOffsetCalls, 1);
    EXPECT_EQ(player.entity.velocityCalls, 0);
    EXPECT_EQ(simulator.initialVelocityCalls, 0);
}

TEST(ManualGrenadeLaunchTest, FallbackPassesExactSpawnAndVelocityInputs)
{
    auto player = makeFallbackPlayer();
    int callOrder{};
    player.entity.callOrder = &callOrder;
    int skipEntity;
    ScriptedFallbackSimulator simulator{.spawnResult = cs2::Vector{4.0f, 5.0f, 6.0f}, .initialVelocity = {7.0f, 8.0f, 9.0f}, .callOrder = &callOrder};

    const auto launch = computeHeldGrenadeLaunchFallback(player, simulator, 0.35f, &skipEntity);

    ASSERT_TRUE(launch.hasValue());
    EXPECT_EQ(simulator.spawnCalls, 1);
    EXPECT_EQ(simulator.spawnEyePosition, (cs2::Vector{100.0f, 200.0f, 364.06f}));
    EXPECT_EQ(simulator.spawnAngles, (cs2::Vector{10.0f, 20.0f, 30.0f}));
    EXPECT_FLOAT_EQ(simulator.spawnStrength, 0.35f);
    EXPECT_EQ(simulator.spawnSkipEntity, &skipEntity);
    EXPECT_EQ(simulator.velocityAngles, simulator.spawnAngles);
    EXPECT_FLOAT_EQ(simulator.baseVelocity, 750.0f);
    EXPECT_FLOAT_EQ(simulator.velocityStrength, 0.35f);
    EXPECT_EQ(simulator.initialVelocityCallOrder, 1);
    EXPECT_EQ(player.entity.velocityCallOrder, 2);
    EXPECT_EQ(launch.value().origin, (cs2::Vector{4.0f, 5.0f, 6.0f}));
    EXPECT_EQ(launch.value().velocity, (cs2::Vector{7.0f, 8.0f, 9.0f}));
}

TEST(ManualGrenadeLaunchTest, FallbackAppliesPlayerVelocityOnlyWhenPresent)
{
    auto player = makeFallbackPlayer();
    ScriptedFallbackSimulator simulator{.spawnResult = cs2::Vector{}, .initialVelocity = {100.0f, 200.0f, 300.0f}};

    const auto withoutPlayerVelocity = computeHeldGrenadeLaunchFallback(player, simulator, 0.5f, nullptr);
    ASSERT_TRUE(withoutPlayerVelocity.hasValue());
    EXPECT_EQ(withoutPlayerVelocity.value().velocity, (cs2::Vector{100.0f, 200.0f, 300.0f}));

    player.entity.velocity = cs2::Vector{4.0f, -8.0f, 12.0f};
    const auto withPlayerVelocity = computeHeldGrenadeLaunchFallback(player, simulator, 0.5f, nullptr);
    ASSERT_TRUE(withPlayerVelocity.hasValue());
    EXPECT_EQ(withPlayerVelocity.value().velocity, (cs2::Vector{105.0f, 190.0f, 315.0f}));
}

TEST(ManualGrenadeLaunchTest, PrefersNativeLaunchWithoutCallingManualProvider)
{
    int nativeCalls{};
    int manualCalls{};
    const GrenadeLaunchState native{{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}};

    const auto launch = prepareGrenadeLaunch(false, true,
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++nativeCalls; return native; },
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++manualCalls; return {}; });

    ASSERT_TRUE(launch.hasValue());
    EXPECT_EQ(launch.value().origin, native.origin);
    EXPECT_EQ(launch.value().velocity, native.velocity);
    EXPECT_EQ(nativeCalls, 1);
    EXPECT_EQ(manualCalls, 0);
}

TEST(ManualGrenadeLaunchTest, FallsBackToManualLaunchWhenNativeIsUnavailable)
{
    const GrenadeLaunchState manual{{7.0f, 8.0f, 9.0f}, {10.0f, 11.0f, 12.0f}};

    const auto launch = prepareGrenadeLaunch(false, true,
        []() noexcept -> Optional<GrenadeLaunchState> { return {}; },
        [&]() noexcept -> Optional<GrenadeLaunchState> { return manual; });

    ASSERT_TRUE(launch.hasValue());
    EXPECT_EQ(launch.value().origin, manual.origin);
    EXPECT_EQ(launch.value().velocity, manual.velocity);
}

TEST(ManualGrenadeLaunchTest, FinalizedLaunchDoesNotInvokeProviders)
{
    int nativeCalls{};
    int manualCalls{};

    const auto launch = prepareGrenadeLaunch(true, true,
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++nativeCalls; return {}; },
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++manualCalls; return {}; });

    EXPECT_FALSE(launch.hasValue());
    EXPECT_EQ(nativeCalls, 0);
    EXPECT_EQ(manualCalls, 0);
}

TEST(ManualGrenadeLaunchTest, UsesFullStrengthUntilPinnedStrengthIsCaptured)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{1};
    static_cast<void>(observation.observeWeapon(weapon));

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);

    static_cast<void>(observation.observePinState(weapon, true));
    observation.retainThrowStrength(0.35f);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.35f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(ManualGrenadeLaunchTest, SkipsAvailableNativeLaunchUntilRealStrengthIsCaptured)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{1};
    int nativeCalls{};
    int manualCalls{};
    static_cast<void>(observation.observeWeapon(weapon));

    const auto manual = prepareGrenadeLaunch(false, observation.hasRetainedThrowStrength,
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++nativeCalls; return GrenadeLaunchState{}; },
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++manualCalls; return GrenadeLaunchState{{1.0f, 0.0f, 0.0f}, {observation.retainedThrowStrength, 0.0f, 0.0f}}; });

    ASSERT_TRUE(manual.hasValue());
    EXPECT_EQ(nativeCalls, 0);
    EXPECT_EQ(manualCalls, 1);
    EXPECT_FLOAT_EQ(manual.value().velocity.x, 1.0f);

    observation.retainThrowStrength(0.5f);
    const auto native = prepareGrenadeLaunch(false, observation.hasRetainedThrowStrength,
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++nativeCalls; return GrenadeLaunchState{{2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}}; },
        [&]() noexcept -> Optional<GrenadeLaunchState> { ++manualCalls; return {}; });

    ASSERT_TRUE(native.hasValue());
    EXPECT_EQ(nativeCalls, 1);
    EXPECT_EQ(manualCalls, 1);
    EXPECT_FLOAT_EQ(native.value().velocity.x, 3.0f);
}

TEST(ManualGrenadeLaunchTest, RetainsPinnedZeroStrength)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{1};
    static_cast<void>(observation.observeWeapon(weapon));
    static_cast<void>(observation.observePinState(weapon, true));
    observation.retainThrowStrength(0.0f);

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.0f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(ManualGrenadeLaunchTest, ComputesReferenceSpawnAndInitialVelocity)
{
    GrenadeSimulatorTestHookContext context;
    context.trace.clearAfterScript();
    Simulator simulator{context};

    const auto spawn = simulator.computeSpawnPosition({10.0f, 20.0f, 64.0f}, {}, 1.0f, nullptr);
    ASSERT_TRUE(spawn.hasValue());
    constexpr float expectedForwardX{0.98480775f};
    constexpr float expectedForwardZ{0.17364818f};
    EXPECT_NEAR(spawn.value().x, 10.0f + expectedForwardX * 16.0f, 0.001f);
    EXPECT_NEAR(spawn.value().y, 20.0f, 0.001f);
    EXPECT_NEAR(spawn.value().z, 64.0f + expectedForwardZ * 16.0f, 0.001f);

    const auto velocity = Simulator::computeInitialVelocity({}, grenade_prediction_params::kBaseThrowVelocity, 1.0f);
    EXPECT_NEAR(velocity.x, 675.0f * expectedForwardX, 0.001f);
    EXPECT_NEAR(velocity.z, 675.0f * expectedForwardZ, 0.001f);
}

}
