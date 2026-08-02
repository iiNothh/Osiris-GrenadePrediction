#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/Held/HeldGrenadeKind.h>

TEST(HeldGrenadeKindTest, MapsExactHeldGrenadeTypes)
{
    EXPECT_EQ(grenade_prediction::heldGrenadeKind({EntityTypeInfo::indexOf<cs2::C_Flashbang>()}), grenade_prediction::GrenadeKind::Flashbang);
    EXPECT_EQ(grenade_prediction::heldGrenadeKind({EntityTypeInfo::indexOf<cs2::C_HEGrenade>()}), grenade_prediction::GrenadeKind::HEGrenade);
    EXPECT_EQ(grenade_prediction::heldGrenadeKind({EntityTypeInfo::indexOf<cs2::C_SmokeGrenade>()}), grenade_prediction::GrenadeKind::SmokeGrenade);
    EXPECT_EQ(grenade_prediction::heldGrenadeKind({EntityTypeInfo::indexOf<cs2::C_MolotovGrenade>()}), grenade_prediction::GrenadeKind::Molotov);
    EXPECT_EQ(grenade_prediction::heldGrenadeKind({EntityTypeInfo::indexOf<cs2::C_IncendiaryGrenade>()}), grenade_prediction::GrenadeKind::Incendiary);
    EXPECT_EQ(grenade_prediction::heldGrenadeKind({EntityTypeInfo::indexOf<cs2::C_DecoyGrenade>()}), grenade_prediction::GrenadeKind::Decoy);
}

TEST(HeldGrenadeKindTest, RejectsProjectilesAndOtherTypes)
{
    EXPECT_EQ(grenade_prediction::heldGrenadeKind({EntityTypeInfo::indexOf<cs2::C_FlashbangProjectile>()}), grenade_prediction::GrenadeKind::None);
    EXPECT_EQ(grenade_prediction::heldGrenadeKind({EntityTypeInfo::indexOf<cs2::C_AK47>()}), grenade_prediction::GrenadeKind::None);
}
