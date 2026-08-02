#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadePrediction.h>

namespace
{

struct HookContext {
};

struct Trace {
    void* ignoredEntity{};
    bool fail{};
    std::size_t calls{};

    [[nodiscard]] Optional<grenade_prediction::GrenadeTraceResult> traceGrenadeHull(cs2::Vector, cs2::Vector, void* ignoredEntity_) noexcept
    {
        ignoredEntity = ignoredEntity_;
        ++calls;
        if (fail)
            return {};
        return grenade_prediction::GrenadeTraceResult{1.0f, {}, {}};
    }
};

struct Renderer {
    std::size_t clearCalls{};
    std::size_t renderCalls{};

    void clear() noexcept { ++clearCalls; }
    void render(cs2::CUIPanel*, const grenade_prediction::GrenadeTrajectory&, const grenade_prediction::GrenadePredictionRenderStyle&, float) noexcept { ++renderCalls; }
};

class LiveGrenadePredictionTest : public testing::Test {
protected:
    HookContext hookContext;
    grenade_prediction::LiveGrenadeCacheState cacheState;
    grenade_prediction::LiveGrenadeAuthorityState authorityState;
    grenade_prediction::LiveGrenadePrediction<HookContext> prediction{hookContext, cacheState, authorityState};
    Trace trace;
    Renderer renderer;
    int pawn;

    void scan(grenade_prediction::LiveGrenadeSnapshot snapshot) noexcept
    {
        prediction.beginScan();
        prediction.observe(snapshot);
        prediction.endScan();
    }

    [[nodiscard]] bool run(cs2::CEntityHandle localPawn = {7}) noexcept
    {
        return prediction.runForLocalPawn(&pawn, localPawn, trace, renderer, nullptr, 1.0f);
    }
};

TEST_F(LiveGrenadePredictionTest, PassesRawLocalPawnToSimulationAndReusesExactAcceptedKey)
{
    scan({{0xFFFF}, {7}, {}, {}, grenade_prediction::GrenadeKind::HEGrenade});

    EXPECT_TRUE(run());
    const auto calls = trace.calls;
    EXPECT_EQ(trace.ignoredEntity, &pawn);
    EXPECT_TRUE(authorityState.accepted);
    EXPECT_EQ(authorityState.acceptedProjectileHandle, (cs2::CEntityHandle{0xFFFF}));
    EXPECT_EQ(authorityState.acceptedThrowerHandle, (cs2::CEntityHandle{7}));

    EXPECT_TRUE(run());
    EXPECT_EQ(trace.calls, calls);
    EXPECT_EQ(renderer.renderCalls, 2U);
}

TEST_F(LiveGrenadePredictionTest, RetriesFailedHighestSequenceAndLeavesHeldEligible)
{
    scan({{9}, {7}, {}, {}, grenade_prediction::GrenadeKind::HEGrenade});
    trace.fail = true;

    EXPECT_FALSE(run());
    EXPECT_FALSE(authorityState.accepted);
    EXPECT_EQ(authorityState.watermark, 1U);
    const auto calls = trace.calls;

    trace.fail = false;
    EXPECT_TRUE(run());
    EXPECT_GT(trace.calls, calls);
    EXPECT_TRUE(authorityState.accepted);
}

TEST_F(LiveGrenadePredictionTest, SuppressesOlderProjectileAfterNewerWatermarkAndMissingAcceptedEntry)
{
    scan({{100}, {7}, {}, {}, grenade_prediction::GrenadeKind::HEGrenade});
    EXPECT_TRUE(run());

    prediction.beginScan();
    prediction.observe({{100}, {7}, {}, {}, grenade_prediction::GrenadeKind::HEGrenade});
    prediction.observe({{1}, {7}, {}, {}, grenade_prediction::GrenadeKind::HEGrenade});
    prediction.endScan();
    trace.fail = true;
    EXPECT_FALSE(run());
    EXPECT_FALSE(authorityState.accepted);
    EXPECT_EQ(authorityState.watermark, 2U);

    prediction.beginScan();
    prediction.observe({{100}, {7}, {}, {}, grenade_prediction::GrenadeKind::HEGrenade});
    prediction.endScan();
    EXPECT_FALSE(run());
    EXPECT_FALSE(authorityState.accepted);
}

TEST_F(LiveGrenadePredictionTest, ClearsForLocalPawnIdentityChangeAndBeforeScanCompletion)
{
    scan({{9}, {7}, {}, {}, grenade_prediction::GrenadeKind::HEGrenade});
    EXPECT_TRUE(run());

    EXPECT_FALSE(run({8}));
    EXPECT_FALSE(authorityState.accepted);
    EXPECT_EQ(authorityState.localPawnIdentity, (cs2::CEntityHandle{8}));

    prediction.beginScan();
    EXPECT_FALSE(run({8}));
    EXPECT_FALSE(authorityState.accepted);
}

}
