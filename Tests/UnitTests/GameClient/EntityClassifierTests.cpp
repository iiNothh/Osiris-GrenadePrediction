#include <gtest/gtest.h>

#include <GameClient/Entities/EntityClassifier.h>

TEST(EntityTypeInfoTest, ClassifiesCZ75AAsWeapon) {
    EXPECT_TRUE((EntityTypeInfo{EntityTypeInfo::indexOf<cs2::C_WeaponCZ75a>()}.isWeapon()));
}
