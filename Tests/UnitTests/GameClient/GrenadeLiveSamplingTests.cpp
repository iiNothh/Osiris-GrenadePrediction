#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

#include <GameClient/Entities/BaseEntity.h>
#include <GameClient/Entities/GrenadeProjectile.h>
#include <GameClient/GlobalVars.h>

namespace
{

constexpr std::size_t kCreateTimeOffset{0x100};
constexpr std::size_t kServerVelocityOffset{0x120};
constexpr std::size_t kAbsOriginOffset{0x180};
constexpr std::size_t kTickCountOffset{0x44};
constexpr std::size_t kTickIntervalOffset{0x48};

struct ProjectileStorage : cs2::C_BaseCSGrenadeProjectile {
    std::array<std::byte, 0x500> storage{};
};

struct GlobalVarsStorage {
    cs2::GlobalVars globalVars{};
    std::array<std::byte, 16> padding{};
    std::int32_t tickCount{};
    float tickInterval{};
};

static_assert(offsetof(GlobalVarsStorage, tickCount) == kTickCountOffset);
static_assert(offsetof(GlobalVarsStorage, tickInterval) == kTickIntervalOffset);

template <typename T>
[[nodiscard]] T* fieldAt(void* object, std::size_t offset) noexcept
{
    return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(object) + offset);
}

struct GrenadeLiveSamplingContext {
    struct PatternSearchResults {
        template <typename>
        [[nodiscard]] static consteval bool supports() noexcept
        {
            return true;
        }

        template <typename T>
        [[nodiscard]] auto get() const noexcept
        {
            if constexpr (std::is_same_v<T, BaseEntityCreateTimeOffset>)
                return BaseEntityCreateTimeOffset::type{context.createTimeAvailable ? static_cast<std::int32_t>(kCreateTimeOffset) : 0};
            else if constexpr (std::is_same_v<T, BaseEntityServerVelocityOffset>)
                return BaseEntityServerVelocityOffset::type{context.serverVelocityAvailable ? static_cast<std::int32_t>(kServerVelocityOffset) : 0};
            else if constexpr (std::is_same_v<T, GetAbsOriginFunction>)
                return context.absOriginAvailable ? &GrenadeLiveSamplingContext::absOrigin : nullptr;
            else if constexpr (std::is_same_v<T, GlobalVarsTickCountOffset>)
                return GlobalVarsTickCountOffset::type{static_cast<std::int8_t>(context.tickCountAvailable ? kTickCountOffset : 0)};
            else {
                static_assert(std::is_same_v<T, GlobalVarsTickIntervalFunction>);
                return context.tickIntervalAvailable ? &GrenadeLiveSamplingContext::tickInterval : nullptr;
            }
        }

        GrenadeLiveSamplingContext& context;
    };

    GrenadeLiveSamplingContext() noexcept
        : results{*this}
    {
    }

    [[nodiscard]] const PatternSearchResults& patternSearchResults() const noexcept
    {
        return results;
    }

    [[nodiscard]] GlobalVars<GrenadeLiveSamplingContext> globalVars() noexcept
    {
        return {*this, globalVarsAvailable ? &globalVarsStorage.globalVars : nullptr};
    }

    static cs2::Vector* absOrigin(cs2::C_BaseEntity* entity) noexcept
    {
        return fieldAt<cs2::Vector>(entity, kAbsOriginOffset);
    }

    static float tickInterval(const cs2::GlobalVars* globalVars) noexcept
    {
        return *fieldAt<float>(const_cast<cs2::GlobalVars*>(globalVars), kTickIntervalOffset);
    }

    PatternSearchResults results;
    GlobalVarsStorage globalVarsStorage{};
    bool createTimeAvailable{true};
    bool serverVelocityAvailable{true};
    bool absOriginAvailable{true};
    bool tickCountAvailable{true};
    bool tickIntervalAvailable{true};
    bool globalVarsAvailable{true};
};

void initializeSamples(GrenadeLiveSamplingContext& context, ProjectileStorage& projectile) noexcept
{
    std::construct_at(fieldAt<float>(&projectile, kCreateTimeOffset), 10.0f);
    std::construct_at(fieldAt<cs2::CNetworkVelocityVector>(&projectile, kServerVelocityOffset), cs2::CNetworkVelocityVector{{4.0f, 5.0f, 6.0f}});
    std::construct_at(fieldAt<cs2::Vector>(&projectile, kAbsOriginOffset), cs2::Vector{1.0f, 2.0f, 3.0f});
    context.globalVarsStorage.globalVars.curtime = 20.0f;
    context.globalVarsStorage.tickCount = 128;
    context.globalVarsStorage.tickInterval = 0.015625f;
}

TEST(GrenadeLiveSamplingTest, ProvidesFiniteCurrentKinematicsAndTiming)
{
    GrenadeLiveSamplingContext context;
    ProjectileStorage projectileStorage;
    initializeSamples(context, projectileStorage);
    const auto baseEntity = BaseEntity{context, static_cast<cs2::C_BaseEntity*>(&projectileStorage)};

    const auto createTime = baseEntity.createTime();
    const auto velocity = baseEntity.serverVelocity();
    const auto origin = baseEntity.absOrigin();
    ASSERT_TRUE(createTime.hasValue());
    ASSERT_TRUE(velocity.hasValue());
    ASSERT_TRUE(origin.hasValue());
    EXPECT_FLOAT_EQ(createTime.value(), 10.0f);
    EXPECT_EQ(velocity.value(), (cs2::Vector{4.0f, 5.0f, 6.0f}));
    EXPECT_EQ(origin.value(), (cs2::Vector{1.0f, 2.0f, 3.0f}));

    const auto timing = context.globalVars().tickInterval();
    const auto worldTime = context.globalVars().curtime();
    const auto tickCount = context.globalVars().tickCount();
    ASSERT_TRUE(timing.hasValue());
    ASSERT_TRUE(worldTime.hasValue());
    ASSERT_TRUE(tickCount.hasValue());
    EXPECT_FLOAT_EQ(timing.value(), 0.015625f);
    EXPECT_FLOAT_EQ(worldTime.value(), 20.0f);
    EXPECT_EQ(tickCount.value(), 128);

    const auto grenade = GrenadeProjectile{context, &projectileStorage};
    const auto sample = grenade.currentSample();
    ASSERT_TRUE(sample.hasValue());
    EXPECT_EQ(sample.value().origin, origin.value());
    EXPECT_EQ(sample.value().velocity, velocity.value());
    EXPECT_FLOAT_EQ(sample.value().worldTime, 20.0f);
    EXPECT_FLOAT_EQ(sample.value().createTime, 10.0f);
    EXPECT_FLOAT_EQ(sample.value().worldTickInterval, 0.015625f);
    EXPECT_EQ(sample.value().worldTickCount, 128);
}

TEST(GrenadeLiveSamplingTest, FailsClosedWhenResolvedPatternsOrGlobalVarsAreUnavailable)
{
    GrenadeLiveSamplingContext context;
    ProjectileStorage projectileStorage;
    initializeSamples(context, projectileStorage);
    const auto baseEntity = BaseEntity{context, static_cast<cs2::C_BaseEntity*>(&projectileStorage)};

    context.createTimeAvailable = false;
    context.serverVelocityAvailable = false;
    context.absOriginAvailable = false;
    context.tickCountAvailable = false;
    context.tickIntervalAvailable = false;
    EXPECT_FALSE(baseEntity.createTime().hasValue());
    EXPECT_FALSE(baseEntity.serverVelocity().hasValue());
    EXPECT_FALSE(baseEntity.absOrigin().hasValue());
    EXPECT_FALSE(context.globalVars().tickCount().hasValue());
    EXPECT_FALSE(context.globalVars().tickInterval().hasValue());

    context.globalVarsAvailable = false;
    EXPECT_FALSE(context.globalVars().curtime().hasValue());
    const auto grenade = GrenadeProjectile{context, &projectileStorage};
    EXPECT_FALSE(grenade.currentSample().hasValue());
}

TEST(GrenadeLiveSamplingTest, RejectsNonFiniteCurrentSamples)
{
    GrenadeLiveSamplingContext context;
    ProjectileStorage projectileStorage;
    initializeSamples(context, projectileStorage);
    *fieldAt<float>(&projectileStorage, kCreateTimeOffset) = std::numeric_limits<float>::quiet_NaN();
    fieldAt<cs2::CNetworkVelocityVector>(&projectileStorage, kServerVelocityOffset)->velocity.y = std::numeric_limits<float>::infinity();
    fieldAt<cs2::Vector>(&projectileStorage, kAbsOriginOffset)->z = std::numeric_limits<float>::quiet_NaN();
    context.globalVarsStorage.globalVars.curtime = std::numeric_limits<float>::infinity();
    context.globalVarsStorage.tickInterval = std::numeric_limits<float>::quiet_NaN();

    const auto baseEntity = BaseEntity{context, static_cast<cs2::C_BaseEntity*>(&projectileStorage)};
    EXPECT_FALSE(baseEntity.createTime().hasValue());
    EXPECT_FALSE(baseEntity.serverVelocity().hasValue());
    EXPECT_TRUE(baseEntity.absOrigin().hasValue());
    EXPECT_TRUE(context.globalVars().curtime().hasValue());
    EXPECT_FALSE(context.globalVars().tickInterval().hasValue());
    const auto grenade = GrenadeProjectile{context, &projectileStorage};
    EXPECT_FALSE(grenade.currentSample().hasValue());
}

}
