#include <type_traits>

#include <gtest/gtest.h>

#include <MemoryPatterns/PatternTypes/EntityPatternTypes.h>
#include <MemoryPatterns/PatternTypes/PlayerPawnPatternTypes.h>
#include <MemoryPatterns/PatternTypes/WeaponPatternTypes.h>

TEST(GrenadeLaunchStatePatternTypesTest, UsesGrenadeOwnedAndPlayerFieldTypes)
{
    EXPECT_TRUE((std::is_same_v<UnpackStrongTypeAliasT<OffsetToThrowStrength>, FieldOffset<cs2::C_BaseCSGrenade, float, std::int32_t>>));
    EXPECT_TRUE((std::is_same_v<UnpackStrongTypeAliasT<OffsetToPinPulled>, FieldOffset<cs2::C_BaseCSGrenade, bool, std::int32_t>>));
    EXPECT_TRUE((std::is_same_v<UnpackStrongTypeAliasT<OffsetToThrowTime>, FieldOffset<cs2::C_BaseCSGrenade, float, std::int32_t>>));
    EXPECT_TRUE((std::is_same_v<UnpackStrongTypeAliasT<OffsetToViewOffset>, FieldOffset<cs2::C_BaseEntity, cs2::Vector, std::int32_t>>));
    EXPECT_TRUE((std::is_same_v<UnpackStrongTypeAliasT<OffsetToAbsVelocity>, FieldOffset<cs2::C_BaseEntity, cs2::Vector, std::int32_t>>));
    EXPECT_TRUE((std::is_same_v<UnpackStrongTypeAliasT<OffsetToEyeAngles>, FieldOffset<cs2::C_CSPlayerPawn, cs2::Vector, std::int32_t>>));
}
