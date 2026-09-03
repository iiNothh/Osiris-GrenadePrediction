#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadeTracePreset.h>

namespace
{

struct RecordingTrace {
    engine_trace::HullTraceRequest genericRequest{};
    engine_trace::HullTraceRequest nativePipRequest{};
    bool genericCalled{};
    bool nativePipCalled{};

    [[nodiscard]] Optional<TraceResult> traceHull(const engine_trace::HullTraceRequest& request) noexcept
    {
        genericRequest = request;
        genericCalled = true;
        return TraceResult{};
    }

    [[nodiscard]] bool isNativePipHullTraceAvailable() const noexcept { return true; }

    [[nodiscard]] Optional<TraceResult> traceNativePipHull(const engine_trace::HullTraceRequest& request) noexcept
    {
        nativePipRequest = request;
        nativePipCalled = true;
        return TraceResult{};
    }
};

void expectLegacyGrenadeRequest(const engine_trace::HullTraceRequest& request, cs2::Vector start, cs2::Vector end,
    void* firstExcluded, void* secondExcluded) noexcept
{
    EXPECT_EQ(request.start, start);
    EXPECT_EQ(request.end, end);
    EXPECT_EQ(request.mins, (cs2::Vector{-2.0f, -2.0f, -2.0f}));
    EXPECT_EQ(request.maxs, (cs2::Vector{2.0f, 2.0f, 2.0f}));
    EXPECT_EQ(request.excludedEntities.first, firstExcluded);
    EXPECT_EQ(request.excludedEntities.second, secondExcluded);
    EXPECT_EQ(request.filter.mask, grenade_trace_preset::MASK_GRENADE);
    EXPECT_EQ(request.filter.collisionGroup, 4);
    EXPECT_EQ(request.filter.queryByte, 7);
}

TEST(GrenadeTracePresetTest, ProducesTheExactLegacyRequest)
{
    std::byte firstExcluded{};
    std::byte secondExcluded{};
    constexpr cs2::Vector start{1.0f, 2.0f, 3.0f};
    constexpr cs2::Vector end{4.0f, 5.0f, 6.0f};

    const auto request = grenade_trace_preset::makeRequest(start, end, {&firstExcluded, &secondExcluded});

    EXPECT_EQ(grenade_trace_preset::MASK_GRENADE, 0x001C200Bull);
    expectLegacyGrenadeRequest(request, start, end, &firstExcluded, &secondExcluded);
    EXPECT_EQ(engine_trace::makeHullTraceDescriptor(request).type, 2u);
}

TEST(GrenadeTracePresetTest, SelectsTheGenericAndNativePipFacadeOperations)
{
    RecordingTrace trace;
    std::byte firstExcluded{};
    std::byte secondExcluded{};
    constexpr cs2::Vector start{1.0f, 2.0f, 3.0f};
    constexpr cs2::Vector end{4.0f, 5.0f, 6.0f};

    ASSERT_TRUE(grenade_trace_preset::traceSpawnHull(trace, start, end, &firstExcluded).hasValue());
    ASSERT_TRUE(grenade_trace_preset::traceInFlightHull(trace, start, end, {&firstExcluded, &secondExcluded}).hasValue());

    EXPECT_TRUE(trace.genericCalled);
    EXPECT_TRUE(trace.nativePipCalled);
    expectLegacyGrenadeRequest(trace.genericRequest, start, end, &firstExcluded, nullptr);
    expectLegacyGrenadeRequest(trace.nativePipRequest, start, end, &firstExcluded, &secondExcluded);
}

TEST(GrenadeTracePresetTest, PreservesSecondExclusionSlotForPaneContinuation)
{
    RecordingTrace trace;
    std::byte pane{};

    ASSERT_TRUE(grenade_trace_preset::traceInFlightHull(trace, {}, {}, {nullptr, &pane}).hasValue());

    EXPECT_TRUE(trace.nativePipCalled);
    expectLegacyGrenadeRequest(trace.nativePipRequest, {}, {}, nullptr, &pane);
}

}
