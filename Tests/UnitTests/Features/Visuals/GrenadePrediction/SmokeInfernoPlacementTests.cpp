#include <limits>

#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/SmokeInfernoPlacement.h>

namespace
{

struct TraceScript {
    Optional<TraceResult> results[16]{};
    engine_trace::HullTraceRequest requests[16]{};
    int resultCount{};
    int calls{};

    void push(Optional<TraceResult> result) noexcept
    {
        results[resultCount++] = result;
    }

    [[nodiscard]] Optional<TraceResult> operator()(const engine_trace::HullTraceRequest& request) noexcept
    {
        requests[calls] = request;
        return calls < resultCount ? results[calls++] : Optional<TraceResult>{};
    }
};

[[nodiscard]] TraceResult traceResult(float fraction, cs2::Vector endPosition = {}) noexcept
{
    return {fraction, endPosition, fraction < 1.0f ? cs2::Vector{0.0f, 0.0f, 1.0f} : cs2::Vector{}};
}

[[nodiscard]] SmokeInfernoSnapshot inferno(cs2::Vector origin, cs2::Vector firstArea, cs2::Vector secondArea = {}) noexcept
{
    SmokeInfernoSnapshot snapshot{
        .origin = origin,
        .fireCount = 1,
        .fireLifetime = 1.0f
    };
    snapshot.firePositions[0] = firstArea;
    snapshot.fireIsBurning[0] = true;
    snapshot.firePositions[1] = secondArea;
    snapshot.fireIsBurning[1] = true;
    return snapshot;
}

[[nodiscard]] SmokeInfernoScan makeScan(const SmokeInfernoSnapshot& first, const SmokeInfernoSnapshot* second = nullptr) noexcept
{
    SmokeInfernoScan scan;
    scan.beginScan();
    scan.observe(first);
    if (second)
        scan.observe(*second);
    static_cast<void>(scan.endScan());
    return scan;
}

TEST(SmokeInfernoPlacementTest, UsesStrictRadiusSeventyVerticalCapsule)
{
    EXPECT_TRUE(SmokeInfernoPlacement::isInsideFireArea({69.9f, 0.0f, 0.0f}, {}));
    EXPECT_FALSE(SmokeInfernoPlacement::isInsideFireArea({70.0f, 0.0f, 0.0f}, {}));
    EXPECT_TRUE(SmokeInfernoPlacement::isInsideFireArea({0.0f, 0.0f, 40.0f}, {}));
    EXPECT_FALSE(SmokeInfernoPlacement::isInsideFireArea({0.0f, 0.0f, 110.0f}, {}));
    EXPECT_FALSE(SmokeInfernoPlacement::isInsideFireArea({std::numeric_limits<float>::infinity(), 0.0f, 0.0f}, {}));
}

TEST(SmokeInfernoPlacementTest, SelectsFirstEligibleFireAreaRatherThanNearest)
{
    auto candidate = inferno({}, {10.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    candidate.fireCount = 2;
    const auto scan = makeScan(candidate);
    TraceScript trace;
    trace.push(traceResult(1.0f));

    const auto target = SmokeInfernoPlacement::selectTarget({}, scan, trace);

    ASSERT_TRUE(target.hasValue());
    EXPECT_EQ(target.value().x, 10.0f);
    EXPECT_EQ(trace.calls, 1);
}

TEST(SmokeInfernoPlacementTest, PreservesTraversalOrderForStrictOuterDistanceTie)
{
    const auto first = inferno({10.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    const auto second = inferno({-10.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f});
    const auto scan = makeScan(first, &second);
    TraceScript trace;
    trace.push(traceResult(1.0f));
    trace.push(traceResult(1.0f));

    const auto target = SmokeInfernoPlacement::selectTarget({}, scan, trace);

    ASSERT_TRUE(target.hasValue());
    EXPECT_EQ(target.value().x, 1.0f);
}

TEST(SmokeInfernoPlacementTest, ShortCircuitsOpenFirstLineOfSightTrace)
{
    const auto scan = makeScan(inferno({}, {}));
    TraceScript trace;
    trace.push(traceResult(1.0f));

    EXPECT_TRUE(SmokeInfernoPlacement::selectTarget({}, scan, trace).hasValue());
    ASSERT_EQ(trace.calls, 1);
    EXPECT_EQ(trace.requests[0].start.z, 70.0f);
    EXPECT_EQ(trace.requests[0].end.z, 0.0f);
    EXPECT_EQ(trace.requests[0].mins, SmokeInfernoPlacement::kPointHullMins);
    EXPECT_EQ(trace.requests[0].filter.interactsExclude.value(), cs2::engine_trace::InteractionLayer::Player | cs2::engine_trace::InteractionLayer::Debris);
}

TEST(SmokeInfernoPlacementTest, InitialLineOfSightUsesOnlyFiniteFraction)
{
    const auto scan = makeScan(inferno({}, {}));
    TraceScript trace;
    trace.push(TraceResult{1.0f, {std::numeric_limits<float>::infinity(), 0.0f, 0.0f},
        {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}});

    EXPECT_TRUE(SmokeInfernoPlacement::selectTarget({}, scan, trace).hasValue());
}

TEST(SmokeInfernoPlacementTest, RetriesLineOfSightOnceFromRaisedTraceEndpoint)
{
    const auto scan = makeScan(inferno({}, {}));
    TraceScript trace;
    trace.push(traceResult(0.5f));
    trace.push(traceResult(0.5f));
    trace.push(traceResult(0.25f, {0.0f, 0.0f, 12.0f}));
    trace.push(traceResult(0.5f));
    trace.push(traceResult(1.0f));

    EXPECT_TRUE(SmokeInfernoPlacement::selectTarget({}, scan, trace).hasValue());
    ASSERT_EQ(trace.calls, 5);
    EXPECT_EQ(trace.requests[2].start.z, 0.0f);
    EXPECT_EQ(trace.requests[2].end.z, 30.0f);
    EXPECT_EQ(trace.requests[3].end.z, 12.0f);
    EXPECT_EQ(trace.requests[4].end.z, 12.0f);
    EXPECT_EQ(trace.requests[3].filter.interactsExclude.value(), cs2::engine_trace::InteractionLayer::Player);
}

TEST(SmokeInfernoPlacementTest, AcceptsFiniteRaisedEndpointDespiteIrrelevantFractionAndNormal)
{
    const auto scan = makeScan(inferno({}, {}));
    TraceScript trace;
    trace.push(traceResult(0.5f));
    trace.push(traceResult(0.5f));
    trace.push(TraceResult{-1.0f, {0.0f, 0.0f, 12.0f}, {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}});
    trace.push(TraceResult{2.0f, {}, {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}});

    EXPECT_TRUE(SmokeInfernoPlacement::selectTarget({}, scan, trace).hasValue());
}

TEST(SmokeInfernoPlacementTest, FailsClosedForUnavailableOrNonfiniteRaisedEndpoint)
{
    const auto scan = makeScan(inferno({}, {}));
    TraceScript unavailableTrace;
    unavailableTrace.push(traceResult(0.5f));
    unavailableTrace.push(traceResult(0.5f));
    unavailableTrace.push(Optional<TraceResult>{});
    EXPECT_FALSE(SmokeInfernoPlacement::selectTarget({}, scan, unavailableTrace).hasValue());

    TraceScript nonfiniteTrace;
    nonfiniteTrace.push(traceResult(0.5f));
    nonfiniteTrace.push(traceResult(0.5f));
    nonfiniteTrace.push(TraceResult{2.0f, {std::numeric_limits<float>::infinity(), 0.0f, 0.0f}, {}});
    EXPECT_FALSE(SmokeInfernoPlacement::selectTarget({}, scan, nonfiniteTrace).hasValue());
}

TEST(SmokeInfernoPlacementTest, RetainsOrdinaryTrajectoryForNoCandidateOrInvalidTrace)
{
    SmokeInfernoScan emptyScan;
    TraceScript noCandidateTrace;
    EXPECT_FALSE(SmokeInfernoPlacement::correctedEndpoint(GrenadeKind::SmokeGrenade, {1.0f, 2.0f, 3.0f}, emptyScan, noCandidateTrace).hasValue());

    const auto scan = makeScan(inferno({}, {}));
    TraceScript invalidTrace;
    invalidTrace.push(Optional<TraceResult>{});
    EXPECT_FALSE(SmokeInfernoPlacement::correctedEndpoint(GrenadeKind::SmokeGrenade, {}, scan, invalidTrace).hasValue());

    auto malformed = inferno({}, {});
    malformed.fireLifetime = std::numeric_limits<float>::quiet_NaN();
    const auto malformedScan = makeScan(malformed);
    TraceScript malformedTrace;
    EXPECT_FALSE(SmokeInfernoPlacement::correctedEndpoint(GrenadeKind::SmokeGrenade, {}, malformedScan, malformedTrace).hasValue());
    EXPECT_EQ(malformedTrace.calls, 0);
}

TEST(SmokeInfernoPlacementTest, AcceptsPositiveInfiniteLifetimeForCandidateCorrection)
{
    auto candidate = inferno({}, {10.0f, 0.0f, 0.0f});
    candidate.fireLifetime = std::numeric_limits<float>::infinity();
    const auto scan = makeScan(candidate);
    TraceScript trace;
    trace.push(traceResult(1.0f));
    trace.push(traceResult(0.25f, {4.0f, 5.0f, 6.0f}));

    EXPECT_TRUE(scan.hasAuthoritativeScan());
    const auto endpoint = SmokeInfernoPlacement::correctedEndpoint(GrenadeKind::SmokeGrenade, {}, scan, trace);
    ASSERT_TRUE(endpoint.hasValue());
    EXPECT_EQ(endpoint.value(), (cs2::Vector{4.0f, 5.0f, 6.0f}));
}

TEST(SmokeInfernoPlacementTest, AcceptsFiniteFinalEndpointDespiteIrrelevantFractionAndNormal)
{
    const auto scan = makeScan(inferno({}, {10.0f, 0.0f, 0.0f}));
    TraceScript trace;
    trace.push(TraceResult{1.0f, {}, {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}});
    trace.push(TraceResult{-1.0f, {4.0f, 5.0f, 6.0f}, {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}});

    const auto endpoint = SmokeInfernoPlacement::correctedEndpoint(GrenadeKind::SmokeGrenade, {}, scan, trace);
    ASSERT_TRUE(endpoint.hasValue());
    EXPECT_EQ(endpoint.value(), (cs2::Vector{4.0f, 5.0f, 6.0f}));
}

TEST(SmokeInfernoPlacementTest, FailsClosedForUnavailableOrNonfiniteFinalEndpoint)
{
    const auto scan = makeScan(inferno({}, {10.0f, 0.0f, 0.0f}));
    TraceScript unavailableTrace;
    unavailableTrace.push(traceResult(1.0f));
    unavailableTrace.push(Optional<TraceResult>{});
    EXPECT_FALSE(SmokeInfernoPlacement::correctedEndpoint(GrenadeKind::SmokeGrenade, {}, scan, unavailableTrace).hasValue());

    TraceScript nonfiniteTrace;
    nonfiniteTrace.push(traceResult(1.0f));
    nonfiniteTrace.push(TraceResult{2.0f, {std::numeric_limits<float>::infinity(), 0.0f, 0.0f}, {}});
    EXPECT_FALSE(SmokeInfernoPlacement::correctedEndpoint(GrenadeKind::SmokeGrenade, {}, scan, nonfiniteTrace).hasValue());
}

TEST(SmokeInfernoPlacementTest, ReturnsCorrectedEndpointOnlyForSmoke)
{
    const auto scan = makeScan(inferno({}, {10.0f, 0.0f, 0.0f}));
    TraceScript smokeTrace;
    smokeTrace.push(traceResult(1.0f));
    smokeTrace.push(traceResult(0.25f, {4.0f, 5.0f, 6.0f}));

    const auto endpoint = SmokeInfernoPlacement::correctedEndpoint(GrenadeKind::SmokeGrenade, {}, scan, smokeTrace);
    ASSERT_TRUE(endpoint.hasValue());
    EXPECT_EQ(endpoint.value().x, 4.0f);
    ASSERT_EQ(smokeTrace.calls, 2);
    EXPECT_EQ(smokeTrace.requests[1].mins, SmokeInfernoPlacement::kFinalHullMins);
    EXPECT_EQ(smokeTrace.requests[1].filter.interactsWith,
        cs2::engine_trace::InteractionLayer::Solid | cs2::engine_trace::InteractionLayer::Window | cs2::engine_trace::InteractionLayer::PassBullets | cs2::engine_trace::InteractionLayer::CsgoGrenadeClip);

    TraceScript flashTrace;
    EXPECT_FALSE(SmokeInfernoPlacement::correctedEndpoint(GrenadeKind::Flashbang, {}, scan, flashTrace).hasValue());
    EXPECT_EQ(flashTrace.calls, 0);
}

}
