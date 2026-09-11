#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include <CS2/Classes/Entities/C_BaseEntity.h>
#include <CS2/Classes/Entities/WeaponEntities.h>
#include <CS2/Classes/EntitySystem/CEntityIdentity.h>
#include <CS2/Classes/Vector.h>
#include <CS2/Constants/EntityHandle.h>
#include <GameClient/Entities/BaseEntity.h>
#include <GameClient/Entities/GrenadeWeapon.h>
#include <MemoryPatterns/PatternTypes/EntityPatternTypes.h>
#include <MemoryPatterns/PatternTypes/WeaponPatternTypes.h>

namespace
{

struct GrenadeWeaponStorage {
    cs2::C_BaseCSGrenade grenadeWeapon{};
    float throwTime{};
    bool pinPulled{};
    float throwStrength{};
};

struct BaseEntityStorage {
    cs2::C_BaseEntity baseEntity{};
    cs2::Vector viewOffset{};
};

struct GrenadeWeaponTestContext {
    struct PatternSearchResults {
        template <typename T>
        [[nodiscard]] auto get() const noexcept
        {
            if constexpr (std::is_same_v<T, OffsetToThrowTime>)
                return GrenadeWeaponOffset<cs2::C_BaseCSGrenade::m_fThrowTime, std::int32_t>{context.throwTimeOffset};
            else if constexpr (std::is_same_v<T, OffsetToPinPulled>)
                return GrenadeWeaponOffset<cs2::C_BaseCSGrenade::m_bPinPulled, std::int32_t>{context.pinPulledOffset};
            else if constexpr (std::is_same_v<T, OffsetToThrowStrength>)
                return GrenadeWeaponOffset<cs2::C_BaseCSGrenade::m_flThrowStrength, std::int32_t>{context.throwStrengthOffset};
            else {
                static_assert(std::is_same_v<T, OffsetToViewOffset>);
                return EntityOffset<cs2::C_BaseEntity::m_vecViewOffset, std::int32_t>{context.viewOffsetOffset};
            }
        }

        GrenadeWeaponTestContext& context;
    };

    GrenadeWeaponTestContext() noexcept
        : results{*this}
    {
    }

    [[nodiscard]] const PatternSearchResults& patternSearchResults() const noexcept
    {
        return results;
    }

    std::int32_t throwTimeOffset{};
    std::int32_t pinPulledOffset{};
    std::int32_t throwStrengthOffset{};
    std::int32_t viewOffsetOffset{};
    PatternSearchResults results;
};

class GrenadeWeaponTest : public testing::Test {
protected:
    void makeOffsetsAvailable() noexcept
    {
        context.throwTimeOffset = offsetOf(&GrenadeWeaponStorage::throwTime, grenadeWeaponStorage);
        context.pinPulledOffset = offsetOf(&GrenadeWeaponStorage::pinPulled, grenadeWeaponStorage);
        context.throwStrengthOffset = offsetOf(&GrenadeWeaponStorage::throwStrength, grenadeWeaponStorage);
        context.viewOffsetOffset = offsetOf(&BaseEntityStorage::viewOffset, baseEntityStorage);
    }

    template <typename Owner, typename Field>
    [[nodiscard]] static std::int32_t offsetOf(Field Owner::*field, const Owner& owner) noexcept
    {
        const auto* const base = reinterpret_cast<const std::byte*>(&owner);
        const auto* const member = reinterpret_cast<const std::byte*>(&(owner.*field));
        return static_cast<std::int32_t>(member - base);
    }

    GrenadeWeaponTestContext context;
    GrenadeWeaponStorage grenadeWeaponStorage{
        .throwTime = 2.5f,
        .pinPulled = true,
        .throwStrength = 0.75f
    };
    BaseEntityStorage baseEntityStorage{
        .viewOffset = {1.0f, 2.0f, 64.0f}
    };
};

TEST_F(GrenadeWeaponTest, AccessorsReturnValuesForAvailableOffsets)
{
    makeOffsetsAvailable();
    GrenadeWeapon grenadeWeapon{context, &grenadeWeaponStorage.grenadeWeapon};
    BaseEntity baseEntity{context, &baseEntityStorage.baseEntity};

    const auto throwTime = grenadeWeapon.throwTime();
    const auto pinPulled = grenadeWeapon.pinPulled();
    const auto throwStrength = grenadeWeapon.throwStrength();
    const auto viewOffset = baseEntity.viewOffset();

    ASSERT_TRUE(throwTime.hasValue());
    ASSERT_TRUE(pinPulled.hasValue());
    ASSERT_TRUE(throwStrength.hasValue());
    ASSERT_TRUE(viewOffset.hasValue());
    EXPECT_EQ(throwTime.value(), 2.5f);
    EXPECT_TRUE(pinPulled.value());
    EXPECT_EQ(throwStrength.value(), 0.75f);
    EXPECT_EQ(viewOffset.value(), (cs2::Vector{1.0f, 2.0f, 64.0f}));
}

TEST_F(GrenadeWeaponTest, AccessorsReturnEmptyForMissingOffsets)
{
    GrenadeWeapon grenadeWeapon{context, &grenadeWeaponStorage.grenadeWeapon};
    BaseEntity baseEntity{context, &baseEntityStorage.baseEntity};

    EXPECT_FALSE(grenadeWeapon.throwTime().hasValue());
    EXPECT_FALSE(grenadeWeapon.pinPulled().hasValue());
    EXPECT_FALSE(grenadeWeapon.throwStrength().hasValue());
    EXPECT_FALSE(baseEntity.viewOffset().hasValue());
}

TEST_F(GrenadeWeaponTest, AccessorsReturnEmptyForNullEntities)
{
    makeOffsetsAvailable();
    GrenadeWeapon grenadeWeapon{context, nullptr};
    BaseEntity baseEntity{context, nullptr};

    EXPECT_FALSE(grenadeWeapon.throwTime().hasValue());
    EXPECT_FALSE(grenadeWeapon.pinPulled().hasValue());
    EXPECT_FALSE(grenadeWeapon.throwStrength().hasValue());
    EXPECT_FALSE(baseEntity.viewOffset().hasValue());
}

TEST_F(GrenadeWeaponTest, HandleIsInvalidWithoutAnEntityIdentity)
{
    BaseEntity entityWithoutIdentity{context, &baseEntityStorage.baseEntity};
    BaseEntity nullEntity{context, nullptr};

    EXPECT_EQ(entityWithoutIdentity.handle(), (cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}));
    EXPECT_EQ(nullEntity.handle(), (cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}));
}

TEST_F(GrenadeWeaponTest, HandleIsInvalidWhenEntityIdentityPointsToADifferentEntity)
{
    cs2::CEntityIdentity identity{
        .entity = &grenadeWeaponStorage.grenadeWeapon
    };
    baseEntityStorage.baseEntity.identity = &identity;
    BaseEntity baseEntity{context, &baseEntityStorage.baseEntity};

    EXPECT_EQ(baseEntity.handle(), (cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}));
}

}
