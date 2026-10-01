#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include <CS2/Constants/StylePropertySymbolNames.h>
#include <CS2/Constants/StylePropertyTypeNames.h>
#include <GameClient/Panorama/PanelStylePropertyFactory.h>
#include <GameClient/Panorama/PanoramaUiPanel.h>
#include <MemoryPatterns/PatternTypes/UiPanelPatternTypes.h>
#include <Platform/Macros/PlatformSpecific.h>
#include <Utils/FieldOffset.h>
#include <Utils/TypeIndex.h>
#include <Utils/TypeList.h>

#if IS_WIN64()
#include <MemoryPatterns/MemoryPatterns.h>
#include <MemorySearch/PatternSearchResults.h>
#endif

namespace
{

using SymbolTypes = decltype(cs2::kStylePropertySymbolNames)::TypeList;
using VmtTypes = decltype(cs2::kStylePropertyTypeNames)::TypeList;
static_assert(std::is_same_v<SymbolTypes, VmtTypes>);
static_assert(cs2::CStylePropertyScale2DCentered{}.m_flScaleX == 1.0f);
static_assert(cs2::CStylePropertyScale2DCentered{}.m_flScaleY == 1.0f);

struct EmptyContext {
};

TEST(PanelStylePropertyFactoryTests, CenteredScaleIsUnavailableWithoutRuntimeSymbolsAndVmt)
{
    EmptyContext context;
    const StylePropertiesSymbolsAndVMTs symbolsAndVmts{};
    const PanelStylePropertyFactory factory{context, symbolsAndVmts};

    EXPECT_FALSE(factory.scale2dCentered(2.0f, 0.5f).has_value());
}

#if IS_WIN64()

template <typename... Patterns>
[[nodiscard]] constexpr std::size_t countLayoutHeightKeys(TypeList<Patterns...>) noexcept
{
    return (std::size_t{} + ... + std::size_t{std::is_same_v<Patterns, GetActualLayoutHeightFunctionOffset>});
}

static_assert(std::is_same_v<GetActualLayoutHeightFunctionOffset::type,
    FieldOffset<const void, cs2::CUIPanel::getActualLayoutHeight, std::int32_t>>);
static_assert(countLayoutHeightKeys(decltype(kClientPatterns)::PatternTypes{}) == 1);
static_assert(countLayoutHeightKeys(decltype(kSceneSystemPatterns)::PatternTypes{}) == 0);
static_assert(countLayoutHeightKeys(decltype(kTier0Patterns)::PatternTypes{}) == 0);
static_assert(countLayoutHeightKeys(decltype(kFileSystemPatterns)::PatternTypes{}) == 0);
static_assert(countLayoutHeightKeys(decltype(kSoundSystemPatterns)::PatternTypes{}) == 0);
static_assert(countLayoutHeightKeys(decltype(kPanoramaPatterns)::PatternTypes{}) == 0);

constexpr std::size_t kSyntheticHeightSlot{3};
constexpr auto kSyntheticHeightOffset = static_cast<std::int32_t>(kSyntheticHeightSlot * sizeof(cs2::CUIPanel::getActualLayoutHeight));

struct LayoutHeightContext {
    GetActualLayoutHeightFunctionOffset::type offset{};

    [[nodiscard]] const auto& patternSearchResults() const noexcept
    {
        return *this;
    }

    template <typename Pattern>
    [[nodiscard]] auto get() const noexcept
    {
        static_assert(std::is_same_v<Pattern, GetActualLayoutHeightFunctionOffset>);
        return offset;
    }
};

TEST(PanelStylePropertyRegistryTests, CenteredScaleUsesMatchingRuntimeSymbolAndRttiEntries)
{
    constexpr auto index = utils::typeIndex<cs2::CStylePropertyScale2DCentered, SymbolTypes>();
    std::vector<std::string_view> symbols;
    std::vector<std::string_view> typeNames;
    cs2::kStylePropertySymbolNames.forEach([&](const char* name) { symbols.emplace_back(name); });
    cs2::kStylePropertyTypeNames.forEach([&](const char* name) { typeNames.emplace_back(name); });

    EXPECT_EQ(symbols[index], "pre-transform-scale2d");
    EXPECT_EQ(typeNames[index], ".?AVCStylePropertyScale2DCentered@panorama@@");
}

TEST(PanoramaUiPanelPrimitivesTests, LayoutHeightIsUnknownForNullPanel)
{
    LayoutHeightContext context{.offset = GetActualLayoutHeightFunctionOffset::type{kSyntheticHeightOffset}};
    EXPECT_FALSE((PanoramaUiPanel{context, nullptr}.getActualLayoutHeight().hasValue()));
}

TEST(PanoramaUiPanelPrimitivesTests, LayoutHeightIsUnknownForNullVmt)
{
    LayoutHeightContext context{.offset = GetActualLayoutHeightFunctionOffset::type{kSyntheticHeightOffset}};
    cs2::CUIPanel panel{};
    EXPECT_FALSE((PanoramaUiPanel{context, &panel}.getActualLayoutHeight().hasValue()));
}

TEST(PanoramaUiPanelPrimitivesTests, LayoutHeightIsUnknownForNullFunction)
{
    LayoutHeightContext context{.offset = GetActualLayoutHeightFunctionOffset::type{kSyntheticHeightOffset}};
    const std::array<cs2::CUIPanel::getActualLayoutHeight, kSyntheticHeightSlot + 1> vmt{};
    cs2::CUIPanel panel{.vmt = vmt.data()};
    EXPECT_FALSE((PanoramaUiPanel{context, &panel}.getActualLayoutHeight().hasValue()));
}

TEST(PanoramaUiPanelPrimitivesTests, LayoutHeightInvokesSuppliedSlotInsteadOfFixedSlot)
{
    LayoutHeightContext context{.offset = GetActualLayoutHeightFunctionOffset::type{kSyntheticHeightOffset}};
    std::array<cs2::CUIPanel::getActualLayoutHeight, 108> vmt{};
    vmt[kSyntheticHeightSlot] = [](cs2::CUIPanel*) { return 37.25f; };
    vmt[107] = [](cs2::CUIPanel*) { return 99.0f; };
    cs2::CUIPanel panel{.vmt = vmt.data()};

    const auto height = PanoramaUiPanel{context, &panel}.getActualLayoutHeight();
    ASSERT_TRUE(height.hasValue());
    EXPECT_FLOAT_EQ(height.value(), 37.25f);
}

TEST(PanoramaUiPanelPrimitivesTests, LayoutHeightIsUnknownForMissingOffsetWithPopulatedVmt)
{
    LayoutHeightContext context;
    std::array<cs2::CUIPanel::getActualLayoutHeight, 108> vmt{};
    vmt.fill([](cs2::CUIPanel*) { ADD_FAILURE(); return 99.0f; });
    cs2::CUIPanel panel{.vmt = vmt.data()};

    EXPECT_FALSE((PanoramaUiPanel{context, &panel}.getActualLayoutHeight().hasValue()));
}

TEST(PanoramaUiPanelPrimitivesTests, ZeroInitializedPatternResultsLeaveLayoutHeightUnavailable)
{
    const PatternSearchResults<decltype(kClientPatterns)> results{};
    LayoutHeightContext context{.offset = results.get<GetActualLayoutHeightFunctionOffset>()};
    std::array<cs2::CUIPanel::getActualLayoutHeight, 108> vmt{};
    vmt.fill([](cs2::CUIPanel*) { ADD_FAILURE(); return 99.0f; });
    cs2::CUIPanel panel{.vmt = vmt.data()};

    EXPECT_EQ(context.offset.value(), 0);
    EXPECT_FALSE((PanoramaUiPanel{context, &panel}.getActualLayoutHeight().hasValue()));
}

struct RecordingStyle : cs2::CPanelStyle {
    int calls{};
    cs2::CStylePropertyScale2DCentered property{};
    bool transition{};
};

struct PanelWithStyle {
    cs2::CUIPanel panel{};
    RecordingStyle style{};
};

void recordSetProperty(cs2::CPanelStyle* style, cs2::CStyleProperty* property, bool transition)
{
    auto& recordingStyle = *static_cast<RecordingStyle*>(style);
    ++recordingStyle.calls;
    recordingStyle.property = *static_cast<cs2::CStylePropertyScale2DCentered*>(property);
    recordingStyle.transition = transition;
}

struct ScalePropertyFactory {
    bool available{true};

    [[nodiscard]] std::optional<cs2::CStylePropertyScale2DCentered> scale2dCentered(float x, float y) const noexcept
    {
        if (!available)
            return {};
        return cs2::CStylePropertyScale2DCentered{.m_flScaleX = x, .m_flScaleY = y};
    }
};

struct PanelContext {
    ScalePropertyFactory factory;
    StylePropertiesSymbolsAndVMTs symbolsAndVmts{};
    std::int8_t styleOffset{static_cast<std::int8_t>(offsetof(PanelWithStyle, style))};
    cs2::CPanelStyle::SetProperty* setter{recordSetProperty};

    [[nodiscard]] const auto& stylePropertySymbolsAndVMTs() const noexcept
    {
        return symbolsAndVmts;
    }

    template <template <typename...> typename Factory>
    [[nodiscard]] auto make(const StylePropertiesSymbolsAndVMTs&) const noexcept
    {
        return factory;
    }

    [[nodiscard]] const auto& patternSearchResults() const noexcept
    {
        return *this;
    }

    template <typename Pattern>
    [[nodiscard]] auto get() const noexcept
    {
        if constexpr (std::is_same_v<Pattern, PanelStyleOffset>)
            return PanelStyleOffset::type{styleOffset};
        else {
            static_assert(std::is_same_v<Pattern, SetPanelStylePropertyFunctionPointer>);
            return setter;
        }
    }
};

enum class UnavailableDependency {
    Factory,
    Panel,
    Style,
    Setter
};

class CenteredScaleUnavailableTests : public testing::TestWithParam<UnavailableDependency> {
};

TEST_P(CenteredScaleUnavailableTests, ReturnsFalseWithoutInvokingEngineSetter)
{
    PanelContext context;
    PanelWithStyle storage;
    auto* panel = &storage.panel;
    switch (GetParam()) {
    case UnavailableDependency::Factory: context.factory.available = false; break;
    case UnavailableDependency::Panel: panel = nullptr; break;
    case UnavailableDependency::Style: context.styleOffset = 0; break;
    case UnavailableDependency::Setter: context.setter = nullptr; break;
    }

    EXPECT_FALSE((PanoramaUiPanel{context, panel}.setScale2dCentered(2.0f, 0.5f)));
    EXPECT_EQ(storage.style.calls, 0);
}

INSTANTIATE_TEST_SUITE_P(Dependencies, CenteredScaleUnavailableTests, testing::Values(
    UnavailableDependency::Factory, UnavailableDependency::Panel,
    UnavailableDependency::Style, UnavailableDependency::Setter));

TEST(PanoramaUiPanelPrimitivesTests, CenteredScaleReturnsTrueOnlyAfterSettingFloatPayload)
{
    PanelContext context;
    PanelWithStyle storage;

    EXPECT_TRUE((PanoramaUiPanel{context, &storage.panel}.setScale2dCentered(2.0f, 0.5f)));
    EXPECT_EQ(storage.style.calls, 1);
    EXPECT_FLOAT_EQ(storage.style.property.m_flScaleX, 2.0f);
    EXPECT_FLOAT_EQ(storage.style.property.m_flScaleY, 0.5f);
    EXPECT_TRUE(storage.style.transition);
}

#else

TEST(PanoramaUiPanelPrimitivesTests, UnverifiedLinuxApisRemainUnavailable)
{
    EmptyContext context;
    cs2::CUIPanel panel{};
    const PanoramaUiPanel wrapper{context, &panel};

    EXPECT_FALSE(wrapper.getActualLayoutHeight().hasValue());
    EXPECT_FALSE(wrapper.setScale2dCentered(2.0f, 0.5f));
}

#endif

}
