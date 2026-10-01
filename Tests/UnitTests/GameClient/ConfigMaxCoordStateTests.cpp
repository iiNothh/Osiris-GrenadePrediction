#include <bit>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include <GameClient/ConfigMaxCoordState.h>
#include <GameClient/DLLs/Tier0Dll.h>
#include <Platform/Macros/IsPlatform.h>
#include <Utils/Optional.h>

namespace
{

static_assert(std::is_same_v<decltype(std::declval<const Tier0Dll&>().configMaxCoordPointer()), float*>);
static_assert(noexcept(std::declval<const Tier0Dll&>().configMaxCoordPointer()));
static_assert(std::is_nothrow_constructible_v<ConfigMaxCoordState, Tier0Dll>);
static_assert(std::is_same_v<decltype(std::declval<const ConfigMaxCoordState&>().current()), Optional<float>>);
static_assert(noexcept(std::declval<const ConfigMaxCoordState&>().current()));

TEST(ConfigMaxCoordStateTests, NullPointerReturnsUnknown)
{
    const ConfigMaxCoordState state{nullptr};

    EXPECT_FALSE(state.current().hasValue());
}

TEST(ConfigMaxCoordStateTests, ReadsCurrentValueAfterPointerMutation)
{
    float configMaxCoord{16384.0f};
    const ConfigMaxCoordState state{&configMaxCoord};

    const auto initial = state.current();
    ASSERT_TRUE(initial.hasValue());
    EXPECT_FLOAT_EQ(initial.value(), 16384.0f);

    configMaxCoord = 8192.0f;

    const auto updated = state.current();
    ASSERT_TRUE(updated.hasValue());
    EXPECT_FLOAT_EQ(updated.value(), 8192.0f);
    EXPECT_FLOAT_EQ(initial.value(), 16384.0f);
}

class ConfigMaxCoordStateValueTests : public testing::TestWithParam<std::uint32_t> {};

TEST_P(ConfigMaxCoordStateValueTests, PreservesValueWithoutValidationOrFallback)
{
    float configMaxCoord{std::bit_cast<float>(GetParam())};
    const ConfigMaxCoordState state{&configMaxCoord};

    const auto current = state.current();

    ASSERT_TRUE(current.hasValue());
    EXPECT_EQ(std::bit_cast<std::uint32_t>(current.value()), GetParam());
}

INSTANTIATE_TEST_SUITE_P(ConfigMaxCoordValues, ConfigMaxCoordStateValueTests, testing::Values(
    0x46800000u,
    0x00000000u,
    0x80000000u,
    0xBF800000u,
    0x7F800000u,
    0xFF800000u,
    0x7FC00000u));

#if IS_LINUX()
static_assert(std::is_nothrow_constructible_v<ConfigMaxCoordState, decltype(std::declval<const Tier0Dll&>().configMaxCoordPointer())>);
#endif

}
