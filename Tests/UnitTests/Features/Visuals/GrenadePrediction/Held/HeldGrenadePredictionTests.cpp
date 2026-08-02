#include <optional>

#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Held/HeldGrenadePrediction.h>

namespace
{

struct HookContext {
};

struct WeaponBaseEntity {
    EntityTypeInfo entityTypeInfo{};

    [[nodiscard]] EntityTypeInfo classify() const noexcept
    {
        return entityTypeInfo;
    }
};

struct Weapon {
    WeaponBaseEntity entity{};
    void* value{};

    [[nodiscard]] const WeaponBaseEntity& baseEntity() const noexcept
    {
        return entity;
    }

    [[nodiscard]] void* raw() const noexcept
    {
        return value;
    }
};

struct PlayerPawn {
    bool exists{true};
    std::optional<bool> alive{true};
    Weapon weapon{};
    void* value{};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return exists;
    }

    [[nodiscard]] std::optional<bool> isAlive() const noexcept
    {
        return alive;
    }

    [[nodiscard]] const Weapon& getActiveWeapon() const noexcept
    {
        return weapon;
    }

    [[nodiscard]] void* raw() const noexcept
    {
        return value;
    }
};

struct NativeLaunchProvider {
    Optional<grenade_prediction::GrenadeLaunchState> launchState{grenade_prediction::GrenadeLaunchState{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -64.0f}}};

    [[nodiscard]] Optional<grenade_prediction::GrenadeLaunchState> get(void*, void*) const noexcept
    {
        return launchState;
    }
};

struct Trace {
    void* ignoredEntity{};
    bool fail{};

    [[nodiscard]] Optional<grenade_prediction::GrenadeTraceResult> traceGrenadeHull(cs2::Vector, cs2::Vector, void* ignoredEntity_) noexcept
    {
        ignoredEntity = ignoredEntity_;
        if (fail)
            return {};
        return grenade_prediction::GrenadeTraceResult{0.5f, {}, {0.0f, 0.0f, 1.0f}};
    }
};

struct Renderer {
    int clearCalls{};
    int renderCalls{};

    void clear() noexcept
    {
        ++clearCalls;
    }

    void render(cs2::CUIPanel*, const grenade_prediction::GrenadeTrajectory&, const grenade_prediction::GrenadePredictionRenderStyle&, float) noexcept
    {
        ++renderCalls;
    }
};

class HeldGrenadePredictionTest : public testing::Test {
protected:
    HookContext hookContext;
    grenade_prediction::HeldGrenadePredictionState state;
    grenade_prediction::HeldGrenadePrediction<HookContext> prediction{hookContext, state};
    PlayerPawn playerPawn;
    NativeLaunchProvider nativeLaunch;
    Trace trace;
    Renderer renderer;

    void makeMolotov() noexcept
    {
        playerPawn.weapon.entity.entityTypeInfo = {EntityTypeInfo::indexOf<cs2::C_MolotovGrenade>()};
        playerPawn.weapon.value = &playerPawn.weapon;
        playerPawn.value = &playerPawn;
    }
};

TEST_F(HeldGrenadePredictionTest, RendersValidOutputAndSkipsTheRawLocalPawn)
{
    makeMolotov();

    prediction.runForPlayer(playerPawn, nativeLaunch, trace, renderer, nullptr, 1.0f);

    EXPECT_EQ(renderer.renderCalls, 1);
    EXPECT_EQ(renderer.clearCalls, 0);
    EXPECT_EQ(trace.ignoredEntity, playerPawn.raw());
    EXPECT_TRUE(state.simulationResult.trajectory.valid);
}

TEST_F(HeldGrenadePredictionTest, ClearsStaleStateForInvalidPawnNonGrenadeFailedLaunchAndFailedSimulation)
{
    state.simulationResult.trajectory.valid = true;
    playerPawn.exists = false;
    prediction.runForPlayer(playerPawn, nativeLaunch, trace, renderer, nullptr, 1.0f);
    EXPECT_FALSE(state.simulationResult.trajectory.valid);

    playerPawn.exists = true;
    prediction.runForPlayer(playerPawn, nativeLaunch, trace, renderer, nullptr, 1.0f);
    EXPECT_FALSE(state.simulationResult.trajectory.valid);

    makeMolotov();
    nativeLaunch.launchState = {};
    prediction.runForPlayer(playerPawn, nativeLaunch, trace, renderer, nullptr, 1.0f);
    EXPECT_FALSE(state.simulationResult.trajectory.valid);

    nativeLaunch.launchState = grenade_prediction::GrenadeLaunchState{{}, {0.0f, 0.0f, -64.0f}};
    trace.fail = true;
    prediction.runForPlayer(playerPawn, nativeLaunch, trace, renderer, nullptr, 1.0f);
    EXPECT_FALSE(state.simulationResult.trajectory.valid);
    EXPECT_EQ(renderer.clearCalls, 4);
}

}
