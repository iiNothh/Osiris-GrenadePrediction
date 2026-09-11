#pragma once

#include <CS2/Classes/Entities/C_BaseEntity.h>
#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <GameClient/Entities/BaseEntity.h>
#include <GameClient/Entities/BaseModelEntity.h>
#include <GameClient/Entities/PlayerPawn.h>
#include <Features/Visuals/ModelGlow/ModelGlow.h>
#include <Features/Visuals/OutlineGlow/OutlineGlow.h>
#include <Features/Visuals/PlayerInfoInWorld/PlayerInfoInWorld.h>
#include <GameClient/EntitySystem/EntitySystem.h>
#include <Features/Hud/BombPlantAlert/BombPlantAlert.h>
#include <Features/Visuals/GrenadePrediction/GrenadePrediction.h>
#include <Platform/GrenadePredictionCapabilities.h>

template <typename HookContext>
class RenderingHookEntityLoop {
public:
    explicit RenderingHookEntityLoop(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void run() const noexcept
    {
        auto bombPlantAlertVisibility = Visibility::Hidden;
        auto* localPawn = static_cast<cs2::C_CSPlayerPawn*>(nullptr);
        cs2::CEntityHandle localPawnHandle{};
        auto grenadePrediction = hookContext.template make<GrenadePrediction>();
        const bool grenadePredictionEnabled = hookContext.config().template getVariable<grenade_prediction_vars::Enabled>();
        const bool shouldScanLiveGrenades = grenadePredictionEnabled
            && (GrenadePredictionPlatformCapabilities::supportsHeldPrediction || GrenadePredictionPlatformCapabilities::supportsLiveProjectilePrediction);
        if (shouldScanLiveGrenades)
            grenadePrediction.beginLiveGrenadeScan();
        hookContext.template make<EntitySystem>().forEachNetworkableEntityIdentity([this, &grenadePrediction, shouldScanLiveGrenades, &bombPlantAlertVisibility, &localPawn, &localPawnHandle](const auto& entityIdentity) {
            handleEntityIdentity(entityIdentity, grenadePrediction, shouldScanLiveGrenades, bombPlantAlertVisibility, localPawn, localPawnHandle);
        });
        if (shouldScanLiveGrenades)
            grenadePrediction.endLiveGrenadeScan(localPawn);
        if (localPawn) {
            auto&& playerPawn = hookContext.template make<PlayerPawn>(localPawn);
            grenadePrediction.handleGrenadePrediction(playerPawn, playerPawn.getActiveWeapon(), localPawnHandle, grenadePredictionEnabled);
        } else {
            grenadePrediction.handleNoLocalPawn();
        }
        hookContext.template make<ModelGlow>().postUpdateInMainThread();
        if (bombPlantAlertVisibility == Visibility::Hidden)
            hookContext.template make<BombPlantAlert>().hide();
    }

private:
    void handleEntityIdentity(const cs2::CEntityIdentity& entityIdentity, GrenadePrediction<HookContext>& grenadePrediction, bool shouldScanLiveGrenades,
        Visibility& bombPlantAlertVisibility, cs2::C_CSPlayerPawn*& localPawn, cs2::CEntityHandle& localPawnHandle) const noexcept
    {
        const auto entityTypeInfo = hookContext.entityClassifier().classifyEntity(entityIdentity.entityClass);
        if (shouldScanLiveGrenades)
            grenadePrediction.updateLiveGrenade(entityIdentity, entityTypeInfo);
        auto&& baseEntity = hookContext.template make<BaseEntity>(static_cast<cs2::C_BaseEntity*>(entityIdentity.entity));

        if (entityTypeInfo.template is<cs2::C_CSPlayerPawn>()) {
            auto&& playerPawn = baseEntity.template as<PlayerPawn>();
            if (playerPawn.isControlledByLocalPlayer()) { localPawn = static_cast<cs2::C_CSPlayerPawn*>(entityIdentity.entity); localPawnHandle = entityIdentity.handle; }
            hookContext.template make<PlayerInfoInWorld>().drawPlayerInformation(playerPawn);
            updateModelGlow<PlayerModelGlow>(playerPawn, entityTypeInfo);
            applyOutlineGlow<PlayerOutlineGlow>(playerPawn, entityTypeInfo);
            if (bombPlantAlertVisibility != Visibility::Visible)
                bombPlantAlertVisibility = hookContext.template make<BombPlantAlert>().show(playerPawn);
        } else if (entityTypeInfo.template is<cs2::C_C4>()) {
            updateModelGlow<DroppedBombModelGlow>(baseEntity.template as<BaseWeapon>(), entityTypeInfo);
            applyOutlineGlow<DroppedBombOutlineGlow>(baseEntity, entityTypeInfo);
        } else if (entityTypeInfo.template is<cs2::CBaseAnimGraph>()) {
            updateModelGlow<DefuseKitModelGlow>(baseEntity, entityTypeInfo);
            applyOutlineGlow<DefuseKitOutlineGlow>(baseEntity, entityTypeInfo);
        } else if (entityTypeInfo.template is<cs2::CPlantedC4>()) {
            updateModelGlow<TickingBombModelGlow>(baseEntity.template as<PlantedC4>(), entityTypeInfo);
            applyOutlineGlow<TickingBombOutlineGlow>(baseEntity.template as<PlantedC4>(), entityTypeInfo);
        }  else if (entityTypeInfo.template is<cs2::C_Hostage>()) {
            applyOutlineGlow<HostageOutlineGlow>(baseEntity, entityTypeInfo);
        } else if (entityTypeInfo.isGrenadeProjectile()) {
            updateModelGlow<GrenadeProjectileModelGlow>(baseEntity, entityTypeInfo);
            applyOutlineGlow<GrenadeProjectileOutlineGlow>(baseEntity, entityTypeInfo);
        } else if (entityTypeInfo.isWeapon()) {
            updateModelGlow<WeaponModelGlow>(baseEntity.template as<BaseWeapon>(), entityTypeInfo);
            applyOutlineGlow<WeaponOutlineGlow>(baseEntity, entityTypeInfo);
        }
    }

    template <template <typename> typename Glow, typename... Args>
    void updateModelGlow(Args&&... args) const
    {
        hookContext.template make<ModelGlow>().updateInMainThread()(Glow{hookContext}, std::forward<Args>(args)...);
    }

    template <template <typename> typename Glow, typename... Args>
    void applyOutlineGlow(Args&&... args) const
    {
        hookContext.template make<OutlineGlow>().applyGlow()(Glow{hookContext}, std::forward<Args>(args)...);
    }

    HookContext& hookContext;
};
