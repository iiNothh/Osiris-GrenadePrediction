#pragma once

#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Held/HeldGrenadePrediction.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeTraceAdapter.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCollector.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadePrediction.h>
#include <GameClient/EngineTrace/EngineTrace.h>
#include <GameClient/Entities/DecoyProjectile.h>
#include <GameClient/Entities/GrenadeProjectile.h>
#include <GameClient/Entities/SmokeGrenadeProjectile.h>
#include <GameClient/WorldToScreen/ViewToProjectionMatrix.h>
#include <MemoryPatterns/PatternTypes/ClientPatternTypes.h>

template <typename HookContext>
class GrenadePrediction {
public:
    explicit GrenadePrediction(HookContext& hookContext) noexcept
        : hookContext{hookContext}
        , heldPrediction{hookContext, hookContext.featuresStates().visualFeaturesStates.grenadePredictionState.held}
        , livePrediction{hookContext, hookContext.featuresStates().visualFeaturesStates.grenadePredictionState.liveCache, hookContext.featuresStates().visualFeaturesStates.grenadePredictionState.liveAuthority}
    {
    }

    void run() noexcept
    {
#if IS_WIN64()
        auto&& playerPawn = hookContext.activeLocalPlayerPawn();
        const auto hudPanel = hookContext.patternSearchResults().template get<HudPanelPointer>();
        if (playerPawn && playerPawn.isAlive().value_or(false) && hudPanel && *hudPanel && (*hudPanel)->uiPanel) {
            EngineTrace<HookContext> engineTrace{hookContext};
            grenade_prediction::GrenadeTraceAdapter trace{engineTrace};
            if (livePrediction.run(playerPawn.raw(), playerPawn.baseEntity().handle(), trace, (*hudPanel)->uiPanel, ViewToProjectionMatrix<HookContext>{hookContext}.getAspectRatio())) {
                heldPrediction.clear();
                return;
            }
        } else if (!playerPawn || !playerPawn.isAlive().value_or(false)) {
            livePrediction.clearAuthority();
        }
#endif
        heldPrediction.run();
    }

    void beginLiveGrenadeScan() noexcept
    {
#if IS_WIN64()
        livePrediction.beginScan();
#endif
    }

    void endLiveGrenadeScan() noexcept
    {
#if IS_WIN64()
        livePrediction.endScan();
#endif
    }

    template <typename BaseEntity, typename EntityTypeInfo>
    void observeLiveGrenade(const BaseEntity& baseEntity, const EntityTypeInfo& entityTypeInfo) noexcept
    {
#if IS_WIN64()
        observe(baseEntity, entityTypeInfo);
#endif
    }

    void clearPrediction() noexcept
    {
        heldPrediction.clear();
        livePrediction.clear();
    }

private:
    template <typename BaseEntity, typename EntityTypeInfo>
    void observe(const BaseEntity& baseEntity, const EntityTypeInfo& entityTypeInfo) noexcept
    {
        const auto handle = baseEntity.handle();
        auto* rawEntity = static_cast<cs2::C_BaseEntity*>(baseEntity);
        auto projectile = hookContext.template make<GrenadeProjectile>(static_cast<cs2::C_BaseCSGrenadeProjectile*>(rawEntity));
        grenade_prediction::GrenadeKind kind{grenade_prediction::GrenadeKind::None};
        if (entityTypeInfo.template is<cs2::C_FlashbangProjectile>()) kind = grenade_prediction::GrenadeKind::Flashbang;
        else if (entityTypeInfo.template is<cs2::C_HEGrenadeProjectile>()) kind = grenade_prediction::GrenadeKind::HEGrenade;
        else if (entityTypeInfo.template is<cs2::C_SmokeGrenadeProjectile>()) {
            const auto smoke = hookContext.template make<SmokeGrenadeProjectile>(static_cast<cs2::C_SmokeGrenadeProjectile*>(rawEntity));
            if (!smoke.didSmokeEffect().hasValue() || smoke.didSmokeEffect().value()) { livePrediction.remove(handle); return; }
            kind = grenade_prediction::GrenadeKind::SmokeGrenade;
        } else if (entityTypeInfo.template is<cs2::C_MolotovProjectile>()) kind = grenade_prediction::GrenadeKind::Molotov;
        else if (entityTypeInfo.template is<cs2::C_DecoyProjectile>()) {
            const auto decoy = hookContext.template make<DecoyProjectile>(static_cast<cs2::C_DecoyProjectile*>(rawEntity));
            if (!decoy.shotTick().hasValue() || decoy.shotTick().value() > 0) { livePrediction.remove(handle); return; }
            kind = grenade_prediction::GrenadeKind::Decoy;
        } else return;
        const auto snapshot = grenade_prediction::LiveGrenadeCollector::collect(projectile, handle, kind);
        if (snapshot.hasValue()) livePrediction.observe(snapshot.value()); else livePrediction.remove(handle);
    }

    HookContext& hookContext;
    grenade_prediction::HeldGrenadePrediction<HookContext> heldPrediction;
    grenade_prediction::LiveGrenadePrediction<HookContext> livePrediction;
};
