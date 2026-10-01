#include <optional>
#include <type_traits>

#include <gtest/gtest.h>

#include <CS2/Classes/EntitySystem/CEntityHandle.h>
#include <CS2/Panorama/PanelHandle.h>
#include <Features/FeaturesStates.h>
#include <GameClient/Entities/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/GrenadePrediction.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionPerHookState.h>
#include <Features/Visuals/GrenadePrediction/Rendering/GrenadeTrajectoryRenderer.h>
#include <GameClient/Panorama/PanoramaUiEngine.h>
#include <Platform/GrenadePredictionCapabilities.h>

namespace
{

struct GrenadePredictionUnloadRendererRecorder {
    cs2::PanelHandle* hiddenPanels[2]{};
    int hideCalls{};
};

struct GrenadePredictionUnloadRenderer {
    GrenadePredictionUnloadRendererRecorder& recorder;

    void hide(cs2::PanelHandle& panelHandle) noexcept
    {
        if (recorder.hideCalls < 2)
            recorder.hiddenPanels[recorder.hideCalls] = &panelHandle;
        ++recorder.hideCalls;
    }
};

struct GrenadePredictionUnloadUiEngine {
    cs2::PanelHandle deletedPanels[2]{};
    int deleteCalls{};

    void deletePanelByHandle(cs2::PanelHandle panelHandle) noexcept
    {
        if (deleteCalls < 2)
            deletedPanels[deleteCalls] = panelHandle;
        ++deleteCalls;
    }
};

struct GrenadePredictionUnloadTestContext {
    GrenadePredictionUnloadTestContext() noexcept
        : renderer{rendererRecorder}
    {
    }

    [[nodiscard]] FeaturesStates& featuresStates() noexcept { return featuresStatesStorage; }

    template <template <typename> typename T, typename... Args>
    [[nodiscard]] decltype(auto) make(Args&&...) noexcept
    {
        if constexpr (std::is_same_v<T<GrenadePredictionUnloadTestContext>, GrenadeTrajectoryRenderer<GrenadePredictionUnloadTestContext>>) {
            return (renderer);
        } else {
            static_assert(std::is_same_v<T<GrenadePredictionUnloadTestContext>, PanoramaUiEngine<GrenadePredictionUnloadTestContext>>);
            return (uiEngine);
        }
    }

    FeaturesStates featuresStatesStorage{};
    GrenadePredictionUnloadRendererRecorder rendererRecorder;
    GrenadePredictionUnloadRenderer renderer;
    GrenadePredictionUnloadUiEngine uiEngine;
};

struct GrenadePredictionBranchRecorder {
    cs2::PanelHandle* hiddenPanels[4]{};
    cs2::PanelHandle* drawnPanels[4]{};
    cs2::Vector drawnFirstPoints[4]{};
    float drawnThicknesses[4]{};
    bool transactionalDraws[4]{};
    int hideCalls{};
    int drawCalls{};
    int pinPulledCalls{};
    int throwStrengthCalls{};
    float gatheredLaunchStrengths[4]{};
    int gatheredLaunchCalls{};
    int fallbackLaunchCalls{};
    int simulateCalls{};
    bool pinPulled{};
    float throwStrength{1.0f};
    GrenadeLaunchState launch{{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}};

    void resetPresentationCalls() noexcept
    {
        hideCalls = 0;
        drawCalls = 0;
    }
};

struct GrenadePredictionBranchRenderer {
    GrenadePredictionBranchRecorder& recorder;

    void hide(cs2::PanelHandle& panelHandle) noexcept
    {
        if (recorder.hideCalls < 4)
            recorder.hiddenPanels[recorder.hideCalls] = &panelHandle;
        ++recorder.hideCalls;
    }

    template <typename Trajectory, typename ParentPanel>
    void draw(const Trajectory& trajectory, cs2::PanelHandle& panelHandle, GrenadeTrajectoryPresentationState&, ParentPanel&&,
        color::Hue, color::Hue, float trajectoryThickness, bool hideWhileUpdating) noexcept
    {
        if (recorder.drawCalls < 4) {
            recorder.drawnPanels[recorder.drawCalls] = &panelHandle;
            recorder.drawnThicknesses[recorder.drawCalls] = trajectoryThickness;
            recorder.transactionalDraws[recorder.drawCalls] = hideWhileUpdating;
            if (trajectory.pointsCount > 0)
                recorder.drawnFirstPoints[recorder.drawCalls] = trajectory.points[0];
        }
        ++recorder.drawCalls;
    }
};

struct GrenadePredictionBranchEngineTrace {
    [[nodiscard]] bool isGrenadeHullTraceAvailable() const noexcept { return true; }
};

struct GrenadePredictionBranchGrenadeWeapon {
    GrenadePredictionBranchRecorder& recorder;

    [[nodiscard]] Optional<float> throwTime() const noexcept { return {}; }
    [[nodiscard]] Optional<bool> pinPulled() const noexcept
    {
        ++recorder.pinPulledCalls;
        return recorder.pinPulled;
    }
    [[nodiscard]] Optional<float> throwStrength() const noexcept
    {
        ++recorder.throwStrengthCalls;
        return recorder.throwStrength;
    }
};

struct GrenadePredictionBranchGatheredGrenadeLaunch {
    GrenadePredictionBranchRecorder& recorder;

    [[nodiscard]] Optional<GrenadeLaunchState> get(cs2::C_BaseCSGrenade*, cs2::C_CSPlayerPawn*, float throwStrength) const noexcept
    {
        if (recorder.gatheredLaunchCalls < 4)
            recorder.gatheredLaunchStrengths[recorder.gatheredLaunchCalls] = throwStrength;
        ++recorder.gatheredLaunchCalls;
        return recorder.launch;
    }
};

struct GrenadePredictionBranchSimulator {
    GrenadePredictionBranchRecorder& recorder;

    void setPlayerCollisionSnapshot(const GrenadePlayerCollisionSnapshot*) const noexcept {}
    [[nodiscard]] Optional<cs2::Vector> computeSpawnPosition(cs2::Vector, cs2::Vector, float, void*) const noexcept
    {
        ++recorder.fallbackLaunchCalls;
        return cs2::Vector{};
    }
    [[nodiscard]] cs2::Vector computeInitialVelocity(cs2::Vector, float, float) const noexcept
    {
        ++recorder.fallbackLaunchCalls;
        return {};
    }
    void simulate(Trajectory& trajectory, const GrenadeLaunchState& launch, GrenadeKind, void*, float) const noexcept
    {
        ++recorder.simulateCalls;
        trajectory.clear();
        trajectory.valid = true;
        trajectory.pointsCount = 2;
        trajectory.points[0] = launch.origin;
        trajectory.points[1] = launch.velocity;
    }
};

struct GrenadePredictionBranchConfig {
    grenade_prediction_vars::LastTrajectoryVisibilityMode visibility{grenade_prediction_vars::LastTrajectoryVisibilityMode::Always};
    grenade_prediction_vars::TrajectoryThickness::ValueType trajectoryThickness{grenade_prediction_vars::TrajectoryThickness::kDefaultValue};

    template <typename Variable>
    [[nodiscard]] auto getVariable() const noexcept
    {
        if constexpr (std::is_same_v<Variable, grenade_prediction_vars::LastTrajectoryVisibility>)
            return visibility;
        else if constexpr (std::is_same_v<Variable, grenade_prediction_vars::TrajectoryThickness>)
            return trajectoryThickness;
        else
            return Variable::kDefaultValue;
    }
};

struct GrenadePredictionBranchGlobalVars {
    Optional<float> currentTime{10.5f};
    Optional<float> frameTime{1.0f};

    [[nodiscard]] Optional<float> curtime() const noexcept { return currentTime; }
    [[nodiscard]] Optional<float> frametime() const noexcept { return frameTime; }
};

struct GrenadePredictionBranchCvarSystem {
    template <typename ConVar>
    [[nodiscard]] auto getConVarValue() const noexcept
    {
        if constexpr (std::is_same_v<ConVar, cs2::sv_grenade_collision_sphere>)
            return std::optional<bool>{true};
        else
            return std::optional<float>{800.0f};
    }
};

struct GrenadePredictionBranchHud {
    [[nodiscard]] int getHudReticle() const noexcept { return 0; }
};

struct GrenadePredictionBranchBaseEntity {
    cs2::C_BaseEntity* entity{};
    cs2::CEntityHandle entityHandle{cs2::INVALID_EHANDLE_INDEX};
    EntityTypeInfo entityType{EntityTypeInfo::indexOf<cs2::C_HEGrenade>()};

    [[nodiscard]] cs2::CEntityHandle handle() const noexcept { return entityHandle; }
    [[nodiscard]] EntityTypeInfo classify() const noexcept { return entityType; }
    [[nodiscard]] Optional<cs2::Vector> viewOffset() const noexcept { return {}; }
    [[nodiscard]] Optional<cs2::Vector> absVelocity() const noexcept { return {}; }
    operator cs2::C_BaseEntity*() const noexcept { return entity; }
};

struct GrenadePredictionBranchPlayerPawn {
    GrenadePredictionBranchBaseEntity baseEntityStorage;

    [[nodiscard]] bool isControlledByLocalPlayer() const noexcept { return true; }
    [[nodiscard]] std::optional<bool> isAlive() const noexcept { return true; }
    [[nodiscard]] const GrenadePredictionBranchBaseEntity& baseEntity() const noexcept { return baseEntityStorage; }
    [[nodiscard]] Optional<cs2::Vector> eyeAngles() const noexcept { return {}; }
    [[nodiscard]] Optional<cs2::Vector> absOrigin() const noexcept { return {}; }
};

struct GrenadePredictionBranchActiveWeapon {
    GrenadePredictionBranchBaseEntity baseEntityStorage;

    [[nodiscard]] const GrenadePredictionBranchBaseEntity& baseEntity() const noexcept { return baseEntityStorage; }
};

struct GrenadePredictionBranchTestContext {
    GrenadePredictionBranchTestContext() noexcept
        : renderer{recorder}
    {
    }

    [[nodiscard]] FeaturesStates& featuresStates() noexcept { return featuresStatesStorage; }
    [[nodiscard]] GrenadePredictionBranchGlobalVars& globalVars() noexcept { return globalVarsStorage; }
    [[nodiscard]] GrenadePredictionBranchConfig& config() noexcept { return configStorage; }
    [[nodiscard]] GrenadePredictionBranchCvarSystem& cvarSystem() noexcept { return cvarSystemStorage; }
    [[nodiscard]] GrenadePredictionBranchHud& hud() noexcept { return hudStorage; }
    [[nodiscard]] GrenadePredictionPerHookState& grenadePredictionPerHookState() noexcept { return perHookState; }

    template <template <typename> typename T, typename... Args>
    [[nodiscard]] decltype(auto) make(Args&&...) noexcept
    {
        if constexpr (std::is_same_v<T<GrenadePredictionBranchTestContext>, GrenadeTrajectoryRenderer<GrenadePredictionBranchTestContext>>) {
            return (renderer);
        } else if constexpr (std::is_same_v<T<GrenadePredictionBranchTestContext>, EngineTrace<GrenadePredictionBranchTestContext>>) {
            return (engineTrace);
        } else if constexpr (std::is_same_v<T<GrenadePredictionBranchTestContext>, GrenadeWeapon<GrenadePredictionBranchTestContext>>) {
            return GrenadePredictionBranchGrenadeWeapon{recorder};
        } else {
            static_assert(std::is_same_v<T<GrenadePredictionBranchTestContext>, GrenadeSimulator<GrenadePredictionBranchTestContext>>);
            return GrenadePredictionBranchSimulator{recorder};
        }
    }

    template <typename T>
    [[nodiscard]] decltype(auto) make() noexcept
    {
        static_assert(std::is_same_v<T, GatheredGrenadeLaunch<GrenadePredictionBranchTestContext>>);
        return GrenadePredictionBranchGatheredGrenadeLaunch{recorder};
    }

    FeaturesStates featuresStatesStorage{};
    GrenadePredictionBranchRecorder recorder;
    GrenadePredictionBranchRenderer renderer;
    GrenadePredictionBranchEngineTrace engineTrace;
    GrenadePredictionBranchGlobalVars globalVarsStorage;
    GrenadePredictionBranchConfig configStorage;
    GrenadePredictionBranchCvarSystem cvarSystemStorage;
    GrenadePredictionBranchHud hudStorage;
    GrenadePredictionPerHookState perHookState;
    cs2::C_BaseCSGrenade grenade{};
};

void armPostThrowSuppression(GrenadePredictionState& state, cs2::CEntityHandle weapon) noexcept
{
    static_cast<void>(state.throwObservation.observeWeapon(weapon));
    static_cast<void>(state.throwObservation.observeThrowTime(weapon, 10.0f));
    static_cast<void>(state.completeHeldThrow(weapon, true, 10.1f));
}

void setCommittedLiveTrajectory(GrenadePredictionState& state) noexcept
{
    constexpr cs2::CEntityHandle localPawn{1};
    constexpr cs2::CEntityHandle projectile{2};
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    state.liveGrenadeAuthority.observeLocalPawn(localPawn);
    static_cast<void>(state.liveGrenadeCache.upsert({projectile, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, GrenadeKind::HEGrenade}));
    const auto liveProjectile = state.liveGrenadeAuthority.newestLocalProjectile(state.liveGrenadeCache);
    if (liveProjectile.hasValue() && state.liveGrenadeAuthority.observeForSimulation(liveProjectile.value()))
        state.liveGrenadeAuthority.accept(liveProjectile.value(), 1.0f);
}

TEST(GrenadePredictionPerHookStateTest, ClearsOnlyTransientScratchBetweenRenderHooks)
{
    GrenadePredictionPerHookState perHookState;
    perHookState.liveGrenadeTrajectoryScratch = {.pointsCount = 1, .valid = true};
    perHookState.playerCollisionCollectionScratch.count = 1;
    perHookState.playerCollisionCollectionScratch.playerDataInvalid = true;
    perHookState.playerCollisionCollectionScratch.overflowed = true;

    FeaturesStates featuresStates;
    auto& state = featuresStates.visualFeaturesStates.grenadePredictionState;
    state.playerCollisionSnapshot = {.count = 1, .status = GrenadePlayerCollisionSnapshotStatus::Available, .revision = 7};
    state.commitLiveGrenadeTrajectory(perHookState.liveGrenadeTrajectoryScratch);

    perHookState.clear();

    EXPECT_FALSE(perHookState.liveGrenadeTrajectoryScratch.valid);
    EXPECT_EQ(perHookState.liveGrenadeTrajectoryScratch.pointsCount, 0);
    EXPECT_EQ(perHookState.playerCollisionCollectionScratch.count, 0);
    EXPECT_FALSE(perHookState.playerCollisionCollectionScratch.playerDataInvalid);
    EXPECT_FALSE(perHookState.playerCollisionCollectionScratch.overflowed);
    EXPECT_EQ(state.playerCollisionSnapshot.status, GrenadePlayerCollisionSnapshotStatus::Available);
    EXPECT_EQ(state.playerCollisionSnapshot.count, 1);
    EXPECT_EQ(state.playerCollisionSnapshot.revision, 7);
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
    EXPECT_EQ(state.lastCommittedTrajectory.pointsCount, 1);
}

TEST(GrenadePredictionTest, OnUnloadClearsStateAndDeletesPanels)
{
    GrenadePredictionUnloadTestContext context;
    auto& state = context.featuresStatesStorage.visualFeaturesStates.grenadePredictionState;
    const cs2::CEntityHandle localPawn{1};
    const cs2::CEntityHandle projectile{2};
    const cs2::PanelHandle livePanel{.panelIndex = 1, .serialNumber = 2};
    const cs2::PanelHandle cachedPanel{.panelIndex = 3, .serialNumber = 4};
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    state.liveContainerPanelHandle = livePanel;
    state.lastCacheContainerPanelHandle = cachedPanel;
    state.livePresentationState.activePanelCount = 1;
    state.livePresentationState.panelStyle.initialized = true;
    state.lastCachePresentationState.activePanelCount = 1;
    state.lastCachePresentationState.panelStyle.initialized = true;
    ASSERT_TRUE(state.liveGrenadeCache.upsert({projectile, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, GrenadeKind::HEGrenade}));

    GrenadePrediction<GrenadePredictionUnloadTestContext>{context}.onUnload();

    EXPECT_EQ(context.rendererRecorder.hideCalls, 2);
    EXPECT_EQ(context.rendererRecorder.hiddenPanels[0], &state.liveContainerPanelHandle);
    EXPECT_EQ(context.rendererRecorder.hiddenPanels[1], &state.lastCacheContainerPanelHandle);
    EXPECT_EQ(context.uiEngine.deleteCalls, 2);
    EXPECT_EQ(context.uiEngine.deletedPanels[0], livePanel);
    EXPECT_EQ(context.uiEngine.deletedPanels[1], cachedPanel);
    EXPECT_FALSE(state.tempTrajectory.valid);
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
    EXPECT_FALSE(state.liveGrenadeCache.newestForThrower(localPawn).hasValue());
    EXPECT_FALSE(state.liveContainerPanelHandle.isValid());
    EXPECT_FALSE(state.lastCacheContainerPanelHandle.isValid());
    EXPECT_EQ(state.livePresentationState.activePanelCount, 0);
    EXPECT_FALSE(state.livePresentationState.panelStyle.initialized);
    EXPECT_EQ(state.lastCachePresentationState.activePanelCount, 0);
    EXPECT_FALSE(state.lastCachePresentationState.panelStyle.initialized);
}

TEST(GrenadePredictionTest, SuppressionAfterInvalidIdentityHidesHeldPredictionWithoutTouchingCommittedLiveState)
{
    if constexpr (!GrenadePredictionPlatformCapabilities::supportsHeldPrediction) {
        GTEST_SKIP();
    } else {
        GrenadePredictionBranchTestContext context;
        auto& state = context.featuresStatesStorage.visualFeaturesStates.grenadePredictionState;
        constexpr cs2::CEntityHandle localPawn{1};
        constexpr cs2::CEntityHandle heldWeapon{4};
        constexpr cs2::CEntityHandle invalidWeapon{cs2::INVALID_EHANDLE_INDEX};
        const cs2::PanelHandle livePanel{.panelIndex = 1, .serialNumber = 2};
        const cs2::PanelHandle cachedPanel{.panelIndex = 3, .serialNumber = 4};
        GrenadePredictionBranchPlayerPawn playerPawn;
        GrenadePredictionBranchActiveWeapon activeWeapon;
        activeWeapon.baseEntityStorage.entityHandle = invalidWeapon;
        state.liveContainerPanelHandle = livePanel;
        state.lastCacheContainerPanelHandle = cachedPanel;
        setCommittedLiveTrajectory(state);
        armPostThrowSuppression(state, heldWeapon);

        GrenadePrediction<GrenadePredictionBranchTestContext> prediction{context};
        prediction.handleGrenadePrediction(playerPawn, activeWeapon, localPawn, true);

        ASSERT_TRUE(state.throwObservation.hasActivePostThrowSuppression());
        state.tempTrajectory.valid = true;
        state.tempTrajectory.pointsCount = 1;
        state.tagTempTrajectory(heldWeapon, state.throwObservation.pendingSequence());
        context.recorder.resetPresentationCalls();
        activeWeapon.baseEntityStorage.entityHandle = heldWeapon;
        context.globalVarsStorage.currentTime = 10.6f;

        prediction.handleGrenadePrediction(playerPawn, activeWeapon, localPawn, true);

        EXPECT_EQ(context.recorder.hideCalls, 1);
        EXPECT_EQ(context.recorder.hiddenPanels[0], &state.liveContainerPanelHandle);
        EXPECT_EQ(context.recorder.drawCalls, 1);
        EXPECT_EQ(context.recorder.drawnPanels[0], &state.lastCacheContainerPanelHandle);
        EXPECT_FLOAT_EQ(context.recorder.drawnThicknesses[0], 2.0f);
        EXPECT_FALSE(state.tempTrajectory.valid);
        EXPECT_EQ(context.recorder.pinPulledCalls, 0);
        EXPECT_EQ(context.recorder.throwStrengthCalls, 0);
        EXPECT_EQ(context.recorder.gatheredLaunchCalls, 0);
        EXPECT_EQ(context.recorder.fallbackLaunchCalls, 0);
        EXPECT_EQ(context.recorder.simulateCalls, 0);
        EXPECT_TRUE(state.throwObservation.hasActivePostThrowSuppression());
        EXPECT_TRUE(state.lastCommittedTrajectory.valid);
        EXPECT_EQ(state.lastCommittedTrajectory.pointsCount, 1);
        EXPECT_TRUE(state.liveGrenadeAuthority.hasAcceptedLiveProjectile());
        EXPECT_TRUE(state.liveGrenadeCache.contains(state.liveGrenadeAuthority.acceptedLiveProjectile()));
    }
}

TEST(GrenadePredictionTest, SwitchingAwayThenBackObservesBothWeaponsAndAllowsHeldLaunchImmediately)
{
    if constexpr (!GrenadePredictionPlatformCapabilities::supportsHeldPrediction) {
        GTEST_SKIP();
    } else {
        GrenadePredictionBranchTestContext context;
        auto& state = context.featuresStatesStorage.visualFeaturesStates.grenadePredictionState;
        constexpr cs2::CEntityHandle localPawn{1};
        constexpr cs2::CEntityHandle originalWeapon{4};
        constexpr cs2::CEntityHandle otherWeapon{5};
        GrenadePredictionBranchPlayerPawn playerPawn;
        GrenadePredictionBranchActiveWeapon activeWeapon;
        activeWeapon.baseEntityStorage.entity = static_cast<cs2::C_BaseEntity*>(&context.grenade);
        armPostThrowSuppression(state, originalWeapon);
        GrenadePrediction<GrenadePredictionBranchTestContext> prediction{context};
        activeWeapon.baseEntityStorage.entityHandle = otherWeapon;
        prediction.handleGrenadePrediction(playerPawn, activeWeapon, localPawn, true);

        ASSERT_FALSE(state.throwObservation.hasActivePostThrowSuppression());
        ASSERT_EQ(state.throwObservation.observedWeapon, otherWeapon);
        ASSERT_EQ(context.recorder.gatheredLaunchCalls, 1);
        EXPECT_FLOAT_EQ(context.recorder.gatheredLaunchStrengths[0], 1.0f);
        context.recorder.pinPulled = true;
        context.recorder.throwStrength = 0.5f;
        activeWeapon.baseEntityStorage.entityHandle = originalWeapon;
        context.globalVarsStorage.currentTime = 10.6f;

        prediction.handleGrenadePrediction(playerPawn, activeWeapon, localPawn, true);

        EXPECT_EQ(state.throwObservation.observedWeapon, originalWeapon);
        EXPECT_FALSE(state.throwObservation.hasActivePostThrowSuppression());
        EXPECT_EQ(context.recorder.throwStrengthCalls, 1);
        EXPECT_EQ(context.recorder.gatheredLaunchCalls, 2);
        EXPECT_FLOAT_EQ(context.recorder.gatheredLaunchStrengths[0], 1.0f);
        EXPECT_FLOAT_EQ(context.recorder.gatheredLaunchStrengths[1], 0.5f);
        EXPECT_EQ(context.recorder.fallbackLaunchCalls, 0);
        EXPECT_EQ(context.recorder.simulateCalls, 2);
    }
}

TEST(GrenadePredictionTest, RecomputesAndPresentsChangedInputOnConsecutiveHighFrequencyCallbacks)
{
    if constexpr (!GrenadePredictionPlatformCapabilities::supportsHeldPrediction) {
        GTEST_SKIP();
    } else {
        GrenadePredictionBranchTestContext context;
        context.globalVarsStorage.frameTime = 1.0f / 240.0f;
        context.configStorage.visibility = grenade_prediction_vars::LastTrajectoryVisibilityMode::Off;
        context.recorder.pinPulled = true;
        GrenadePredictionBranchPlayerPawn playerPawn;
        GrenadePredictionBranchActiveWeapon activeWeapon;
        activeWeapon.baseEntityStorage.entity = static_cast<cs2::C_BaseEntity*>(&context.grenade);
        activeWeapon.baseEntityStorage.entityHandle = cs2::CEntityHandle{4};
        constexpr cs2::CEntityHandle localPawn{1};
        GrenadePrediction<GrenadePredictionBranchTestContext> prediction{context};

        prediction.handleGrenadePrediction(playerPawn, activeWeapon, localPawn, true);
        ASSERT_EQ(context.recorder.simulateCalls, 1);
        ASSERT_EQ(context.recorder.drawCalls, 1);

        context.recorder.launch.origin = {10.0f, 20.0f, 30.0f};
        context.recorder.launch.velocity = {40.0f, 50.0f, 60.0f};
        prediction.handleGrenadePrediction(playerPawn, activeWeapon, localPawn, true);

        EXPECT_EQ(context.recorder.simulateCalls, 2);
        EXPECT_EQ(context.recorder.drawCalls, 2);
        EXPECT_EQ(context.recorder.drawnFirstPoints[1].x, 10.0f);
        EXPECT_EQ(context.recorder.drawnFirstPoints[1].y, 20.0f);
        EXPECT_EQ(context.recorder.drawnFirstPoints[1].z, 30.0f);
        EXPECT_TRUE(context.recorder.transactionalDraws[0]);
        EXPECT_TRUE(context.recorder.transactionalDraws[1]);
        EXPECT_EQ(context.recorder.drawnPanels[0], &context.featuresStatesStorage.visualFeaturesStates.grenadePredictionState.liveContainerPanelHandle);
        EXPECT_EQ(context.recorder.drawnPanels[1], &context.featuresStatesStorage.visualFeaturesStates.grenadePredictionState.liveContainerPanelHandle);
    }
}

TEST(GrenadePredictionTest, ReusesTrajectoryForIdenticalInputOnConsecutiveHighFrequencyCallbacks)
{
    if constexpr (!GrenadePredictionPlatformCapabilities::supportsHeldPrediction) {
        GTEST_SKIP();
    } else {
        GrenadePredictionBranchTestContext context;
        context.globalVarsStorage.frameTime = 1.0f / 240.0f;
        context.configStorage.visibility = grenade_prediction_vars::LastTrajectoryVisibilityMode::Off;
        context.recorder.pinPulled = true;
        GrenadePredictionBranchPlayerPawn playerPawn;
        GrenadePredictionBranchActiveWeapon activeWeapon;
        activeWeapon.baseEntityStorage.entity = static_cast<cs2::C_BaseEntity*>(&context.grenade);
        activeWeapon.baseEntityStorage.entityHandle = cs2::CEntityHandle{4};
        constexpr cs2::CEntityHandle localPawn{1};
        GrenadePrediction<GrenadePredictionBranchTestContext> prediction{context};

        prediction.handleGrenadePrediction(playerPawn, activeWeapon, localPawn, true);
        ASSERT_EQ(context.recorder.simulateCalls, 1);
        prediction.handleGrenadePrediction(playerPawn, activeWeapon, localPawn, true);

        EXPECT_EQ(context.recorder.simulateCalls, 1);
        EXPECT_EQ(context.recorder.drawCalls, 2);
        EXPECT_FLOAT_EQ(context.recorder.drawnThicknesses[0], 2.0f);
        EXPECT_FLOAT_EQ(context.recorder.drawnThicknesses[1], 2.0f);
        EXPECT_TRUE(context.recorder.transactionalDraws[0]);
        EXPECT_TRUE(context.recorder.transactionalDraws[1]);
        EXPECT_EQ(context.recorder.drawnFirstPoints[0].x, context.recorder.drawnFirstPoints[1].x);
        EXPECT_EQ(context.recorder.drawnFirstPoints[0].y, context.recorder.drawnFirstPoints[1].y);
        EXPECT_EQ(context.recorder.drawnFirstPoints[0].z, context.recorder.drawnFirstPoints[1].z);
    }
}

TEST(GrenadePredictionTest, HidesUnownedLiveTrajectoryInSameCallbackAndKeepsCachedTrajectoryVisible)
{
    if constexpr (!GrenadePredictionPlatformCapabilities::supportsHeldPrediction) {
        GTEST_SKIP();
    } else {
        GrenadePredictionBranchTestContext context;
        context.configStorage.trajectoryThickness = grenade_prediction_vars::TrajectoryThickness::ValueType{1.234f};
        context.recorder.pinPulled = true;
        GrenadePredictionBranchPlayerPawn playerPawn;
        GrenadePredictionBranchActiveWeapon activeWeapon;
        activeWeapon.baseEntityStorage.entity = static_cast<cs2::C_BaseEntity*>(&context.grenade);
        activeWeapon.baseEntityStorage.entityHandle = cs2::CEntityHandle{4};
        constexpr cs2::CEntityHandle localPawn{1};
        auto& state = context.featuresStatesStorage.visualFeaturesStates.grenadePredictionState;
        GrenadePrediction<GrenadePredictionBranchTestContext> prediction{context};

        prediction.handleGrenadePrediction(playerPawn, activeWeapon, localPawn, true);
        ASSERT_EQ(context.recorder.drawCalls, 1);
        EXPECT_FLOAT_EQ(context.recorder.drawnThicknesses[0], 1.23f);
        ASSERT_TRUE(state.tempTrajectory.valid);

        state.lastCommittedTrajectory = state.tempTrajectory;
        constexpr cs2::CEntityHandle otherPawn{2};
        constexpr cs2::CEntityHandle projectile{3};
        ASSERT_TRUE(state.liveGrenadeCache.upsert({projectile, otherPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, GrenadeKind::HEGrenade}));
        const auto observedProjectile = state.liveGrenadeCache.newestForThrower(otherPawn);
        ASSERT_TRUE(observedProjectile.hasValue());
        state.liveGrenadeAuthority.observeLocalPawn(localPawn);
        ASSERT_TRUE(state.liveGrenadeAuthority.observeForSimulation(observedProjectile.value()));
        const int previousHideCalls = context.recorder.hideCalls;
        prediction.handleGrenadePrediction(playerPawn, activeWeapon, localPawn, true);

        EXPECT_FALSE(state.tempTrajectory.valid);
        ASSERT_GT(context.recorder.hideCalls, previousHideCalls);
        EXPECT_EQ(context.recorder.hiddenPanels[previousHideCalls], &state.liveContainerPanelHandle);
        EXPECT_EQ(context.recorder.drawCalls, 2);
        EXPECT_EQ(context.recorder.drawnPanels[1], &state.lastCacheContainerPanelHandle);
        EXPECT_FLOAT_EQ(context.recorder.drawnThicknesses[1], 1.23f);
        EXPECT_FALSE(context.recorder.transactionalDraws[1]);
        EXPECT_TRUE(state.lastCommittedTrajectory.valid);
    }
}

TEST(GrenadePredictionPlatformCapabilitiesTest, ReportsLiveProjectileSupportThroughPlatformCapabilities)
{
    EXPECT_EQ(GrenadePredictionPlatformCapabilities::supportsLiveProjectilePrediction, IS_WIN64());
    EXPECT_EQ(GrenadePredictionPlatformCapabilities::supportsHeldPrediction, IS_WIN64());
    EXPECT_EQ(GrenadePredictionPlatformCapabilities::supportsGatheredHeldLaunch, IS_WIN64());
}

}
