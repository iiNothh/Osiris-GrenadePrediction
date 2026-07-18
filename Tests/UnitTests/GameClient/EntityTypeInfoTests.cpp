#include <gtest/gtest.h>

#include <GameClient/Entities/EntityClassifier.h>
#include <Features/Visuals/GrenadePrediction/GrenadeKindMapper.h>

TEST(EntityTypeInfoTest, RecognizesCZ75aAsWeapon)
{
    EXPECT_TRUE(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_WeaponCZ75a>()}.isWeapon());
}

TEST(EntityTypeInfoTest, RecognizesSemanticWeaponLeavesAsWeapons)
{
    EXPECT_TRUE(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_WeaponP250>()}.isWeapon());
    EXPECT_TRUE(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_WeaponTec9>()}.isWeapon());
    EXPECT_TRUE(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_WeaponUMP45>()}.isWeapon());
    EXPECT_TRUE(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_WeaponP90>()}.isWeapon());
    EXPECT_TRUE(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_WeaponSawedoff>()}.isWeapon());
    EXPECT_TRUE(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_WeaponXM1014>()}.isWeapon());
    EXPECT_TRUE(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_WeaponSG556>()}.isWeapon());
    EXPECT_TRUE(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_WeaponSSG08>()}.isWeapon());
    EXPECT_TRUE(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_WeaponSCAR20>()}.isWeapon());
}

TEST(EntityTypeInfoTest, MapsExactGrenadeClassesToGrenadeKinds)
{
    EXPECT_EQ(GrenadeKindMapper::from(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_Flashbang>()}), cs2::GrenadeKind::Flashbang);
    EXPECT_EQ(GrenadeKindMapper::from(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_HEGrenade>()}), cs2::GrenadeKind::HEGrenade);
    EXPECT_EQ(GrenadeKindMapper::from(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_SmokeGrenade>()}), cs2::GrenadeKind::SmokeGrenade);
    EXPECT_EQ(GrenadeKindMapper::from(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_MolotovGrenade>()}), cs2::GrenadeKind::Molotov);
    EXPECT_EQ(GrenadeKindMapper::from(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_IncendiaryGrenade>()}), cs2::GrenadeKind::Incendiary);
    EXPECT_EQ(GrenadeKindMapper::from(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_DecoyGrenade>()}), cs2::GrenadeKind::Decoy);
    EXPECT_EQ(GrenadeKindMapper::from(EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_WeaponGlock>()}), cs2::GrenadeKind::None);
    EXPECT_EQ(GrenadeKindMapper::from(EntityTypeInfo{}), cs2::GrenadeKind::None);
}
