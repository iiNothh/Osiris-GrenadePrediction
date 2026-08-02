#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include <CS2/Classes/EntitySystem/CEntityIdentity.h>
#include <CS2/Constants/EntityHandle.h>
#include <GameClient/GrenadePrediction/GrenadeLaunch.h>

#if defined(_WIN64)
namespace
{

static_assert(std::is_same_v<UnpackStrongTypeAliasT<BuildGrenadeLaunchFunction>, char(*)(cs2::C_CSWeaponBase*, cs2::C_CSPlayerPawn*, cs2::Vector*, cs2::Vector*, bool)>);

struct CallState {
    int count{};
    cs2::C_CSWeaponBase* weapon{};
    cs2::C_CSPlayerPawn* playerPawn{};
    bool mode{true};
    bool outputsWereNonFinite{};
    char returnValue{};
    bool writeOrigin{true};
    bool writeVelocity{true};
    cs2::Vector origin{1.0f, 2.0f, 3.0f};
    cs2::Vector velocity{4.0f, 5.0f, 6.0f};
};

struct WeaponStorage {
    cs2::C_CSWeaponBase weapon{};
    cs2::CEntityHandle ownerHandle{};
    CallState* callState{};
};

char buildGrenadeLaunch(cs2::C_CSWeaponBase* weapon, cs2::C_CSPlayerPawn* playerPawn, cs2::Vector* origin, cs2::Vector* velocity, bool mode) noexcept
{
    auto& state = *reinterpret_cast<WeaponStorage*>(weapon)->callState;
    ++state.count;
    state.weapon = weapon;
    state.playerPawn = playerPawn;
    state.mode = mode;
    state.outputsWereNonFinite = !Math::isFinite(origin->x) && !Math::isFinite(origin->y) && !Math::isFinite(origin->z)
        && !Math::isFinite(velocity->x) && !Math::isFinite(velocity->y) && !Math::isFinite(velocity->z);
    if (state.writeOrigin)
        *origin = state.origin;
    if (state.writeVelocity)
        *velocity = state.velocity;
    return state.returnValue;
}

struct PatternResults {
    bool provideBuilder{true};
    bool provideOwnerOffset{true};
    std::int32_t ownerOffset{static_cast<std::int32_t>(offsetof(WeaponStorage, ownerHandle))};

    template <typename T>
    auto get() const noexcept
    {
        if constexpr (std::is_same_v<T, BuildGrenadeLaunchFunction>)
            return provideBuilder ? &buildGrenadeLaunch : nullptr;
        else
            return provideOwnerOffset ? UnpackStrongTypeAliasT<OffsetToOwnerEntity>{ownerOffset} : UnpackStrongTypeAliasT<OffsetToOwnerEntity>{};
    }
};

struct HookContext {
    [[nodiscard]] const PatternResults& patternSearchResults() const noexcept
    {
        return results;
    }

    PatternResults results{};
};

class GrenadeLaunchTest : public testing::Test {
protected:
    void SetUp() override
    {
        weaponStorage.callState = &callState;
        playerPawn.identity = &identity;
        identity.handle = ownerHandle;
        weaponStorage.ownerHandle = ownerHandle;
    }

    HookContext hookContext;
    CallState callState;
    WeaponStorage weaponStorage;
    cs2::C_CSPlayerPawn playerPawn{};
    cs2::CEntityIdentity identity{};
    cs2::CEntityHandle ownerHandle{42};
};

TEST_F(GrenadeLaunchTest, PassesAbiInputsAndReturnsFiniteStateWhenCharReturnIsZero)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};
    callState.returnValue = 0;

    const auto result = grenadeLaunch.get(&weaponStorage.weapon, &playerPawn);

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().origin, callState.origin);
    EXPECT_EQ(result.value().velocity, callState.velocity);
    EXPECT_EQ(callState.count, 1);
    EXPECT_EQ(callState.weapon, &weaponStorage.weapon);
    EXPECT_EQ(callState.playerPawn, &playerPawn);
    EXPECT_FALSE(callState.mode);
    EXPECT_TRUE(callState.outputsWereNonFinite);
}

TEST_F(GrenadeLaunchTest, ReturnsFiniteStateWhenCharReturnIsOne)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};
    callState.returnValue = 1;

    const auto result = grenadeLaunch.get(&weaponStorage.weapon, &playerPawn);

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().origin, callState.origin);
    EXPECT_EQ(result.value().velocity, callState.velocity);
    EXPECT_EQ(callState.count, 1);
}

TEST_F(GrenadeLaunchTest, RejectsNullInputsAndMissingPawnIdentity)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};

    EXPECT_FALSE(grenadeLaunch.get(nullptr, &playerPawn).hasValue());
    EXPECT_FALSE(grenadeLaunch.get(&weaponStorage.weapon, nullptr).hasValue());
    playerPawn.identity = nullptr;
    EXPECT_FALSE(grenadeLaunch.get(&weaponStorage.weapon, &playerPawn).hasValue());
    EXPECT_EQ(callState.count, 0);
}

TEST_F(GrenadeLaunchTest, RejectsMissingBuilderWithoutNativeCall)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};
    hookContext.results.provideBuilder = false;

    EXPECT_FALSE(grenadeLaunch.get(&weaponStorage.weapon, &playerPawn).hasValue());
    EXPECT_EQ(callState.count, 0);
}

TEST_F(GrenadeLaunchTest, RejectsMissingOwnerOffsetWithoutNativeCall)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};
    hookContext.results.provideOwnerOffset = false;

    EXPECT_FALSE(grenadeLaunch.get(&weaponStorage.weapon, &playerPawn).hasValue());
    EXPECT_EQ(callState.count, 0);
}

TEST_F(GrenadeLaunchTest, RejectsUnreadableOwnerWithoutNativeCall)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};
    hookContext.results.ownerOffset = 0;

    EXPECT_FALSE(grenadeLaunch.get(&weaponStorage.weapon, &playerPawn).hasValue());
    EXPECT_EQ(callState.count, 0);
}

TEST_F(GrenadeLaunchTest, RejectsInvalidWeaponOwnerWithoutNativeCall)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};
    weaponStorage.ownerHandle = {cs2::INVALID_EHANDLE_INDEX};

    EXPECT_FALSE(grenadeLaunch.get(&weaponStorage.weapon, &playerPawn).hasValue());
    EXPECT_EQ(callState.count, 0);
}

TEST_F(GrenadeLaunchTest, RejectsInvalidPawnHandleWithoutNativeCall)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};
    identity.handle = {cs2::INVALID_EHANDLE_INDEX};

    EXPECT_FALSE(grenadeLaunch.get(&weaponStorage.weapon, &playerPawn).hasValue());
    EXPECT_EQ(callState.count, 0);
}

TEST_F(GrenadeLaunchTest, RejectsMismatchedHandlesWithoutNativeCall)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};
    weaponStorage.ownerHandle = {43};

    EXPECT_FALSE(grenadeLaunch.get(&weaponStorage.weapon, &playerPawn).hasValue());
    EXPECT_EQ(callState.count, 0);
}

TEST_F(GrenadeLaunchTest, RejectsPartialOutputWriteAfterOneNativeCall)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};
    callState.writeVelocity = false;

    EXPECT_FALSE(grenadeLaunch.get(&weaponStorage.weapon, &playerPawn).hasValue());
    EXPECT_EQ(callState.count, 1);
}

TEST_F(GrenadeLaunchTest, RejectsNaNOutputAfterOneNativeCall)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};
    callState.origin.x = std::bit_cast<float>(std::uint32_t{0x7FC00000u});

    EXPECT_FALSE(grenadeLaunch.get(&weaponStorage.weapon, &playerPawn).hasValue());
    EXPECT_EQ(callState.count, 1);
}

TEST_F(GrenadeLaunchTest, RejectsPositiveInfinityOutputAfterOneNativeCall)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};
    callState.velocity.y = std::bit_cast<float>(std::uint32_t{0x7F800000u});

    EXPECT_FALSE(grenadeLaunch.get(&weaponStorage.weapon, &playerPawn).hasValue());
    EXPECT_EQ(callState.count, 1);
}

TEST_F(GrenadeLaunchTest, RejectsNegativeInfinityOutputAfterOneNativeCall)
{
    GrenadeLaunch<HookContext> grenadeLaunch{hookContext};
    callState.velocity.z = std::bit_cast<float>(std::uint32_t{0xFF800000u});

    EXPECT_FALSE(grenadeLaunch.get(&weaponStorage.weapon, &playerPawn).hasValue());
    EXPECT_EQ(callState.count, 1);
}

}
#endif
