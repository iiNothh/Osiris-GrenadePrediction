#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

#include <gtest/gtest.h>

#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Classes/Entities/WeaponEntities.h>
#include <CS2/Classes/EntitySystem/CEntityIdentity.h>
#include <CS2/Constants/EntityHandle.h>
#include <GameClient/GrenadePrediction/UnpinnedGrenadeLaunch.h>
#include <MemoryPatterns/PatternTypes/ClientPatternTypes.h>
#include <MemoryPatterns/PatternTypes/EngineTracePatternTypes.h>
#include <MemoryPatterns/PatternTypes/EntityPatternTypes.h>

namespace
{

constexpr cs2::CEntityHandle kPawnHandle{1};

enum class GatherOutput {
    Untouched,
    NonFiniteViewAngles,
    NonFiniteSource,
    NonFiniteCenter,
    NonFiniteMovement,
    Finite
};

struct UnpinnedLaunchRecorder {
    int gatherCalls{};
    int filterConstructionCalls{};
    int buildQueryShapeCalls{};
    int traceShapeCalls{};
    GatherOutput gatherOutput{GatherOutput::Finite};
    char gatherReturn{};
    bool traceReturn{true};
    bool writeEndpoint{true};
    cs2::C_CSPlayerPawn* pawn{};
    bool gatherFinalArgument{};
    void* filterOwner{};
    cs2::engine_trace::InteractionLayer interactionMask{};
    cs2::CollisionGroup collisionGroup{};
    cs2::PhysicsQueryFlag queryFlags{};
    cs2::AABB_t bounds{};
    cs2::Vector traceStart{};
    cs2::Vector traceEnd{};
};

UnpinnedLaunchRecorder* activeRecorder{};

class ActiveRecorderGuard {
public:
    explicit ActiveRecorderGuard(UnpinnedLaunchRecorder& recorder) noexcept
        : recorder{recorder}
    {
        activeRecorder = &recorder;
    }

    ~ActiveRecorderGuard()
    {
        if (activeRecorder == &recorder)
            activeRecorder = nullptr;
    }

    ActiveRecorderGuard(const ActiveRecorderGuard&) = delete;
    ActiveRecorderGuard& operator=(const ActiveRecorderGuard&) = delete;

private:
    UnpinnedLaunchRecorder& recorder;
};

char gatherGrenadeLaunchInputs(cs2::C_CSPlayerPawn* pawn, cs2::Vector* viewAngles, cs2::Vector* source, cs2::Vector* center, cs2::Vector* movement, bool finalArgument) noexcept
{
    if (activeRecorder == nullptr)
        return {};

    ++activeRecorder->gatherCalls;
    activeRecorder->pawn = pawn;
    activeRecorder->gatherFinalArgument = finalArgument;
    if (activeRecorder->gatherOutput == GatherOutput::Untouched)
        return activeRecorder->gatherReturn;

    *viewAngles = {};
    *source = {100.0f, 200.0f, 300.0f};
    *center = {10.0f, 20.0f, 30.0f};
    *movement = {4.0f, -8.0f, 12.0f};
    constexpr float nonFinite = std::bit_cast<float>(std::uint32_t{0x7FC00000u});
    switch (activeRecorder->gatherOutput) {
    case GatherOutput::Untouched: break;
    case GatherOutput::NonFiniteViewAngles: viewAngles->x = nonFinite; break;
    case GatherOutput::NonFiniteSource: source->x = nonFinite; break;
    case GatherOutput::NonFiniteCenter: center->x = nonFinite; break;
    case GatherOutput::NonFiniteMovement: movement->x = nonFinite; break;
    case GatherOutput::Finite: break;
    }
    return activeRecorder->gatherReturn;
}

[[nodiscard]] cs2::CTraceFilter* constructFilter(cs2::CTraceFilter* filter, void* owner, cs2::engine_trace::InteractionLayer interactionMask,
    cs2::CollisionGroup collisionGroup, cs2::PhysicsQueryFlag queryFlags) noexcept
{
    if (activeRecorder != nullptr) {
        ++activeRecorder->filterConstructionCalls;
        activeRecorder->filterOwner = owner;
        activeRecorder->interactionMask = interactionMask;
        activeRecorder->collisionGroup = collisionGroup;
        activeRecorder->queryFlags = queryFlags;
    }
    return filter;
}

void buildQueryShape(cs2::RnQueryShapeAttr_t*, const cs2::AABB_t* bounds) noexcept
{
    if (activeRecorder != nullptr) {
        ++activeRecorder->buildQueryShapeCalls;
        activeRecorder->bounds = *bounds;
    }
}

template <typename T>
void writeOutput(cs2::CGameTrace& output, std::size_t offset, T value) noexcept
{
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    for (std::size_t i{}; i < sizeof(T); ++i)
        output.storage[offset + i] = bytes[i];
}

[[nodiscard]] bool traceShape(cs2::PhysicsWorldPointerSlot, const cs2::RnQueryShapeAttr_t*, const cs2::Vector* start, const cs2::Vector* end,
    cs2::CTraceFilter*, cs2::CGameTrace* output) noexcept
{
    if (activeRecorder != nullptr) {
        ++activeRecorder->traceShapeCalls;
        activeRecorder->traceStart = *start;
        activeRecorder->traceEnd = *end;
        if (activeRecorder->writeEndpoint)
            writeOutput(*output, 0x10, cs2::Vector{7.0f, 8.0f, 9.0f});
        return activeRecorder->traceReturn;
    }
    return false;
}

struct TestCvarSystem {
    template <typename ConVar>
    [[nodiscard]] std::optional<bool> getConVarValue() const noexcept
    {
        static_assert(std::is_same_v<ConVar, cs2::sv_grenade_collision_sphere>);
        return collisionSphere;
    }

    std::optional<bool> collisionSphere{true};
};

struct GrenadeStorage {
    cs2::C_BaseCSGrenade grenade{};
    cs2::CEntityHandle owner{};
};

struct UnpinnedGrenadeLaunchTestContext {
    struct PatternSearchResults {
        template <typename T>
        [[nodiscard]] auto get() const noexcept
        {
            if constexpr (std::is_same_v<T, GatherGrenadeLaunchInputsFunction>)
                return context.gatherAvailable ? &gatherGrenadeLaunchInputs : nullptr;
            else if constexpr (std::is_same_v<T, OffsetToOwnerEntity>)
                return EntityOffset<cs2::C_BaseEntity::m_hOwnerEntity, std::int32_t>{context.ownerOffset};
            else if constexpr (std::is_same_v<T, TraceShapeFunctionPointer>)
                return context.traceAvailable ? &traceShape : nullptr;
            else if constexpr (std::is_same_v<T, BuildRnQueryShapeAttrFromAABBFunctionPointer>)
                return context.traceAvailable ? &buildQueryShape : nullptr;
            else if constexpr (std::is_same_v<T, PhysicsWorldPointerSlotStoragePointer>)
                return context.traceAvailable ? &context.physicsWorldPointerSlot : nullptr;
            else if constexpr (std::is_same_v<T, CTraceFilterConstructionFunctionPointer>)
                return context.traceAvailable ? &constructFilter : nullptr;
            else {
                static_assert(std::is_same_v<T, CGameTraceEndPositionOffset>);
                return CGameTraceOffset<cs2::Vector, std::int32_t>{context.traceAvailable ? 0x10 : 0};
            }
        }

        UnpinnedGrenadeLaunchTestContext& context;
    };

    UnpinnedGrenadeLaunchTestContext() noexcept
        : results{*this}
    {
    }

    [[nodiscard]] const PatternSearchResults& patternSearchResults() const noexcept { return results; }
    [[nodiscard]] TestCvarSystem& cvarSystem() noexcept { return cvars; }
    [[nodiscard]] const TestCvarSystem& cvarSystem() const noexcept { return cvars; }

    std::byte physicsWorldObject{};
    cs2::IVPhysics2World* physicsWorld{reinterpret_cast<cs2::IVPhysics2World*>(&physicsWorldObject)};
    cs2::PhysicsWorldPointerSlot physicsWorldPointerSlot{&physicsWorld};
    bool gatherAvailable{true};
    bool traceAvailable{true};
    std::int32_t ownerOffset{};
    TestCvarSystem cvars;
    PatternSearchResults results;
};

class UnpinnedGrenadeLaunchTest : public testing::Test {
protected:
    void makeLaunchAvailable() noexcept
    {
        context.ownerOffset = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(&grenade.owner) - reinterpret_cast<std::uintptr_t>(&grenade.grenade));
        grenade.owner = kPawnHandle;
        pawn.identity = &identity;
    }

    UnpinnedLaunchRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    UnpinnedGrenadeLaunchTestContext context;
    GrenadeStorage grenade;
    cs2::CEntityIdentity identity{.handle = kPawnHandle};
    cs2::C_CSPlayerPawn pawn{};
};

static_assert(std::is_same_v<cs2::C_CSPlayerPawn::GatherGrenadeLaunchInputs, char(cs2::C_CSPlayerPawn*, cs2::Vector*, cs2::Vector*, cs2::Vector*, cs2::Vector*, bool)>);
static_assert(std::is_same_v<UnpackStrongTypeAliasT<GatherGrenadeLaunchInputsFunction>, cs2::C_CSPlayerPawn::GatherGrenadeLaunchInputs*>);

TEST_F(UnpinnedGrenadeLaunchTest, IsUnavailableWithoutTheGatherPattern)
{
    makeLaunchAvailable();
    context.gatherAvailable = false;

    EXPECT_FALSE(UnpinnedGrenadeLaunch{context}.get(&grenade.grenade, &pawn).hasValue());
    EXPECT_EQ(recorder.gatherCalls, 0);
    EXPECT_EQ(recorder.traceShapeCalls, 0);
}

TEST_F(UnpinnedGrenadeLaunchTest, RejectsNullInputsAndMissingOrInvalidOwner)
{
    makeLaunchAvailable();

    EXPECT_FALSE(UnpinnedGrenadeLaunch{context}.get(nullptr, &pawn).hasValue());
    EXPECT_FALSE(UnpinnedGrenadeLaunch{context}.get(&grenade.grenade, nullptr).hasValue());
    pawn.identity = nullptr;
    EXPECT_FALSE(UnpinnedGrenadeLaunch{context}.get(&grenade.grenade, &pawn).hasValue());

    pawn.identity = &identity;
    context.ownerOffset = {};
    EXPECT_FALSE(UnpinnedGrenadeLaunch{context}.get(&grenade.grenade, &pawn).hasValue());
    makeLaunchAvailable();
    grenade.owner = cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX};
    EXPECT_FALSE(UnpinnedGrenadeLaunch{context}.get(&grenade.grenade, &pawn).hasValue());
    grenade.owner = cs2::CEntityHandle{2};
    EXPECT_FALSE(UnpinnedGrenadeLaunch{context}.get(&grenade.grenade, &pawn).hasValue());
    EXPECT_EQ(recorder.gatherCalls, 0);
}

TEST_F(UnpinnedGrenadeLaunchTest, UsesFiniteGatheredInputsAndIgnoresTheNativeReturnByte)
{
    if constexpr (!GrenadePredictionPlatformCapabilities::supportsNativeUnpinnedHeldLaunch) {
        EXPECT_FALSE(UnpinnedGrenadeLaunch{context}.get(&grenade.grenade, &pawn).hasValue());
    } else {
        makeLaunchAvailable();
        recorder.gatherReturn = 1;

        const auto launch = UnpinnedGrenadeLaunch{context}.get(&grenade.grenade, &pawn);

        ASSERT_TRUE(launch.hasValue());
        EXPECT_EQ(recorder.gatherCalls, 1);
        EXPECT_EQ(recorder.pawn, &pawn);
        EXPECT_FALSE(recorder.gatherFinalArgument);
        EXPECT_EQ(recorder.traceShapeCalls, 0);
        EXPECT_EQ(launch.value().origin, (cs2::Vector{100.0f, 200.0f, 300.0f}));
    }
}

TEST_F(UnpinnedGrenadeLaunchTest, RejectsUnwrittenAndEveryNonFiniteGatherOutput)
{
    if constexpr (GrenadePredictionPlatformCapabilities::supportsNativeUnpinnedHeldLaunch) {
        makeLaunchAvailable();
        for (const auto output : {GatherOutput::Untouched, GatherOutput::NonFiniteViewAngles, GatherOutput::NonFiniteSource,
                 GatherOutput::NonFiniteCenter, GatherOutput::NonFiniteMovement}) {
            recorder.gatherOutput = output;
            EXPECT_FALSE(UnpinnedGrenadeLaunch{context}.get(&grenade.grenade, &pawn).hasValue());
        }
        EXPECT_EQ(recorder.gatherCalls, 5);
        EXPECT_EQ(recorder.traceShapeCalls, 0);
    }
}

TEST(UnpinnedGrenadeLaunchFinalizationTest, AppliesFixedFullStrengthSpeedSourceAndMovement)
{
    const auto launch = UnpinnedGrenadeLaunch<UnpinnedGrenadeLaunchTestContext>::finalize({100.0f, 200.0f, 300.0f}, {}, {4.0f, -8.0f, 12.0f});

    ASSERT_TRUE(launch.hasValue());
    EXPECT_EQ(launch.value().origin, (cs2::Vector{100.0f, 200.0f, 300.0f}));
    EXPECT_NEAR(launch.value().velocity.x, 675.0f * 0.98480775f + 5.0f, 0.001f);
    EXPECT_NEAR(launch.value().velocity.y, -10.0f, 0.001f);
    EXPECT_NEAR(launch.value().velocity.z, 675.0f * 0.17364818f + 15.0f, 0.001f);
}

TEST(UnpinnedGrenadeLaunchFinalizationTest, WrapsPitchOnlyOnceBeforeCompensation)
{
    const auto above = UnpinnedGrenadeLaunch<UnpinnedGrenadeLaunchTestContext>::finalize({}, {91.0f, 0.0f, 0.0f}, {});
    const auto below = UnpinnedGrenadeLaunch<UnpinnedGrenadeLaunchTestContext>::finalize({}, {-91.0f, 0.0f, 0.0f}, {});
    const auto upperBoundary = UnpinnedGrenadeLaunch<UnpinnedGrenadeLaunchTestContext>::finalize({}, {90.0f, 0.0f, 0.0f}, {});
    const auto lowerBoundary = UnpinnedGrenadeLaunch<UnpinnedGrenadeLaunchTestContext>::finalize({}, {-90.0f, 0.0f, 0.0f}, {});

    ASSERT_TRUE(above.hasValue());
    ASSERT_TRUE(below.hasValue());
    ASSERT_TRUE(upperBoundary.hasValue());
    ASSERT_TRUE(lowerBoundary.hasValue());
    EXPECT_NEAR(above.value().velocity.x, -240.6757f, 0.001f);
    EXPECT_NEAR(above.value().velocity.z, -630.6348f, 0.001f);
    EXPECT_NEAR(below.value().velocity.x, 218.5203f, 0.001f);
    EXPECT_NEAR(below.value().velocity.z, 638.6500f, 0.001f);
    EXPECT_NEAR(upperBoundary.value().velocity.z, -675.0f, 0.001f);
    EXPECT_NEAR(lowerBoundary.value().velocity.z, 675.0f, 0.001f);
}

TEST_F(UnpinnedGrenadeLaunchTest, UsesCollisionSphereConVarAndFailsClosedWhenUnavailable)
{
    if constexpr (GrenadePredictionPlatformCapabilities::supportsNativeUnpinnedHeldLaunch) {
        makeLaunchAvailable();
        context.cvars.collisionSphere = true;
        EXPECT_TRUE(UnpinnedGrenadeLaunch{context}.get(&grenade.grenade, &pawn).hasValue());
        EXPECT_EQ(recorder.traceShapeCalls, 0);

        context.cvars.collisionSphere = false;
        const auto traced = UnpinnedGrenadeLaunch{context}.get(&grenade.grenade, &pawn);
        ASSERT_TRUE(traced.hasValue());
        EXPECT_EQ(traced.value().origin, (cs2::Vector{7.0f, 8.0f, 9.0f}));
        EXPECT_EQ(recorder.traceShapeCalls, 1);
        EXPECT_EQ(recorder.filterOwner, &pawn);
        EXPECT_EQ(recorder.traceStart, (cs2::Vector{10.0f, 20.0f, 30.0f}));
        EXPECT_NEAR(recorder.traceEnd.x, 100.0f + 0.98480775f * 16.0f, 0.001f);
        EXPECT_NEAR(recorder.traceEnd.y, 200.0f, 0.001f);
        EXPECT_NEAR(recorder.traceEnd.z, 300.0f + 0.17364818f * 16.0f, 0.001f);

        context.cvars.collisionSphere = {};
        EXPECT_FALSE(UnpinnedGrenadeLaunch{context}.get(&grenade.grenade, &pawn).hasValue());
        EXPECT_EQ(recorder.traceShapeCalls, 1);
    }
}

}
