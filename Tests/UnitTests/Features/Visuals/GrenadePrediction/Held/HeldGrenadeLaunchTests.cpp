#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Held/HeldGrenadeLaunch.h>

namespace
{

struct WeaponBaseEntity {
    EntityTypeInfo entityTypeInfo{};

    [[nodiscard]] EntityTypeInfo classify() const noexcept
    {
        return entityTypeInfo;
    }
};

struct Weapon {
    WeaponBaseEntity entity{};
    void* weapon{};

    [[nodiscard]] const WeaponBaseEntity& baseEntity() const noexcept
    {
        return entity;
    }

    [[nodiscard]] void* raw() const noexcept
    {
        return weapon;
    }
};

struct PlayerPawn {
    Weapon weapon{};
    void* pawn{};

    [[nodiscard]] const Weapon& getActiveWeapon() const noexcept
    {
        return weapon;
    }

    [[nodiscard]] void* raw() const noexcept
    {
        return pawn;
    }
};

struct NativeLaunchProvider {
    int calls{};
    void* weapon{};
    void* pawn{};
    Optional<grenade_prediction::GrenadeLaunchState> result{grenade_prediction::GrenadeLaunchState{{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}}};

    [[nodiscard]] Optional<grenade_prediction::GrenadeLaunchState> get(void* weapon_, void* pawn_) noexcept
    {
        ++calls;
        weapon = weapon_;
        pawn = pawn_;
        return result;
    }
};

TEST(HeldGrenadeLaunchTest, RejectsBeforeCallingNativeProvider)
{
    PlayerPawn playerPawn{};
    NativeLaunchProvider nativeLaunch{};

    EXPECT_FALSE(grenade_prediction::HeldGrenadeLaunch{}.get(playerPawn, nativeLaunch).hasValue());
    EXPECT_EQ(nativeLaunch.calls, 0);
}

TEST(HeldGrenadeLaunchTest, PreparesNativeLaunchAfterHeldClassification)
{
    PlayerPawn playerPawn{};
    playerPawn.weapon.entity.entityTypeInfo = {EntityTypeInfo::indexOf<cs2::C_HEGrenade>()};
    playerPawn.weapon.weapon = &playerPawn.weapon;
    playerPawn.pawn = &playerPawn;
    NativeLaunchProvider nativeLaunch{};

    const auto launch = grenade_prediction::HeldGrenadeLaunch{}.get(playerPawn, nativeLaunch);

    ASSERT_TRUE(launch.hasValue());
    EXPECT_EQ(launch.value().kind, grenade_prediction::GrenadeKind::HEGrenade);
    EXPECT_EQ(nativeLaunch.calls, 1);
    EXPECT_EQ(nativeLaunch.weapon, playerPawn.weapon.weapon);
    EXPECT_EQ(nativeLaunch.pawn, playerPawn.pawn);
}

TEST(HeldGrenadeLaunchTest, RejectsClassifiedGrenadeWithoutRawWeapon)
{
    PlayerPawn playerPawn{};
    playerPawn.weapon.entity.entityTypeInfo = {EntityTypeInfo::indexOf<cs2::C_HEGrenade>()};
    NativeLaunchProvider nativeLaunch{};

    EXPECT_FALSE(grenade_prediction::HeldGrenadeLaunch{}.get(playerPawn, nativeLaunch).hasValue());
    EXPECT_EQ(nativeLaunch.calls, 0);
}

}
