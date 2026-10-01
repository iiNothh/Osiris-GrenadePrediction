#include <any>
#include <cstddef>
#include <limits>
#include <set>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <Config/ConfigParams.h>
#include <Config/ConfigSchema.h>
#include <Config/ConfigVariableTypes.h>

#include <Features/Visuals/GrenadePrediction/GrenadePredictionConfigVariables.h>

#include <Mocks/MockConfig.h>
#include <Mocks/MockConfigConversion.h>
#include <Mocks/MockHookContext.h>

class ConfigSchemaTest : public testing::Test {
protected:
    testing::StrictMock<MockHookContext> mockHookContext;
    testing::StrictMock<MockConfigConversion> mockConfigConversion;
    testing::StrictMock<MockConfig> mockConfig;

    ConfigSchema<MockHookContext> configSchema{mockHookContext};
    using IndexInNestingLevel = std::size_t;
    std::vector<IndexInNestingLevel> nestingLevels;
    std::set<std::size_t> configVariableIndexes;
};

TEST_F(ConfigSchemaTest, SchemaIsValid) {
    EXPECT_CALL(mockConfigConversion, beginRoot()).WillOnce([this] {
        EXPECT_EQ(nestingLevels.size(), 0);
        EXPECT_LE(nestingLevels.size(), config_params::kMaxNestingLevel);
        nestingLevels.push_back(IndexInNestingLevel{});
    });

    EXPECT_CALL(mockConfigConversion, endRoot()).WillOnce([this] {
        EXPECT_EQ(nestingLevels.size(), 1);
        nestingLevels.clear();
    });

    EXPECT_CALL(mockConfigConversion, beginObject(testing::_)).WillRepeatedly([this] {
        EXPECT_GT(nestingLevels.size(), 0);
        EXPECT_LE(nestingLevels.size(), config_params::kMaxNestingLevel);
        nestingLevels.push_back(IndexInNestingLevel{});
    });

    EXPECT_CALL(mockConfigConversion, endObject()).WillRepeatedly([this] {
        EXPECT_GT(nestingLevels.size(), 1);
        nestingLevels.pop_back();
        EXPECT_LT(nestingLevels.back(), config_params::kMaxObjectIndex);
        ++nestingLevels.back();
    });

    EXPECT_CALL(mockConfigConversion, boolean(testing::_, testing::_, testing::_)).WillRepeatedly([this] {
        EXPECT_GT(nestingLevels.size(), 0);
        EXPECT_LT(nestingLevels.back(), config_params::kMaxObjectIndex);
        ++nestingLevels.back();
    });

    EXPECT_CALL(mockConfigConversion, uint(testing::_, testing::_, testing::_)).WillRepeatedly([this] {
        EXPECT_GT(nestingLevels.size(), 0);
        EXPECT_LT(nestingLevels.back(), config_params::kMaxObjectIndex);
        ++nestingLevels.back();
    });
    EXPECT_CALL(mockConfigConversion, floatValue(testing::_, testing::_, testing::_)).WillRepeatedly(testing::Invoke([this] {
        EXPECT_GT(nestingLevels.size(), 0);
        EXPECT_LT(nestingLevels.back(), config_params::kMaxObjectIndex);
        ++nestingLevels.back();
    }));

    configSchema.performConversion(mockConfigConversion);
}

TEST_F(ConfigSchemaTest, EachConfigVariableIsLoadedOnce) {
    EXPECT_CALL(mockConfigConversion, beginRoot());
    EXPECT_CALL(mockConfigConversion, endRoot());
    EXPECT_CALL(mockConfigConversion, beginObject(testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(mockConfigConversion, endObject()).Times(testing::AnyNumber());

    EXPECT_CALL(mockHookContext, config()).WillRepeatedly(testing::ReturnRef(mockConfig));

    EXPECT_CALL(mockConfigConversion, boolean(testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::WithArg<1>([this](auto valueSetter) {
            EXPECT_CALL(mockConfig, setVariableWithoutAutoSave(testing::_, testing::_))
                .WillOnce(testing::WithArg<0>([this](std::size_t configVariableIndex) {
                    EXPECT_FALSE(configVariableIndexes.contains(configVariableIndex));
                    configVariableIndexes.insert(configVariableIndex);
                }));
            valueSetter(bool{}); 
        }));

    EXPECT_CALL(mockConfigConversion, uint(testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::WithArg<1>([this](auto valueSetter) {
            EXPECT_CALL(mockConfig, setVariableWithoutAutoSave(testing::_, testing::_))
                .WillOnce(testing::WithArg<0>([this](std::size_t configVariableIndex) {
                    EXPECT_FALSE(configVariableIndexes.contains(configVariableIndex));
                    configVariableIndexes.insert(configVariableIndex);
                }));
            valueSetter(std::uint64_t{}); 
        }));
    EXPECT_CALL(mockConfigConversion, floatValue(testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::WithArg<1>([this](auto valueSetter) {
            EXPECT_CALL(mockConfig, setVariableWithoutAutoSave(testing::_, testing::_))
                .WillOnce(testing::WithArg<0>([this](std::size_t configVariableIndex) {
                    EXPECT_FALSE(configVariableIndexes.contains(configVariableIndex));
                    configVariableIndexes.insert(configVariableIndex);
                }));
            valueSetter(0.0f);
        }));

    configSchema.performConversion(mockConfigConversion);
    EXPECT_EQ(configVariableIndexes.size(), ConfigVariableTypes::size());
}

TEST_F(ConfigSchemaTest, NormalizesLoadedGrenadePredictionCacheDuration) {
    EXPECT_CALL(mockConfigConversion, beginRoot());
    EXPECT_CALL(mockConfigConversion, endRoot());
    EXPECT_CALL(mockConfigConversion, beginObject(testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(mockConfigConversion, endObject()).Times(testing::AnyNumber());
    EXPECT_CALL(mockConfigConversion, boolean(testing::_, testing::_, testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(mockConfigConversion, uint(testing::_, testing::_, testing::_)).Times(testing::AnyNumber());

    EXPECT_CALL(mockHookContext, config()).WillOnce(testing::ReturnRef(mockConfig));
    EXPECT_CALL(mockConfigConversion, floatValue(testing::Eq(std::u8string_view{u8"Thickness"}), testing::_, testing::_));
    EXPECT_CALL(mockConfigConversion, floatValue(testing::Eq(std::u8string_view{u8"CacheDuration"}), testing::_, testing::_))
        .WillOnce(testing::WithArgs<0, 1>([this](const char8_t* id, auto valueSetter) {
            EXPECT_EQ(std::u8string_view{id}, u8"CacheDuration");
            EXPECT_CALL(mockConfig, setVariableWithoutAutoSave(ConfigVariableTypes::indexOf<grenade_prediction_vars::CacheDuration>(), testing::_))
                .WillOnce(testing::WithArg<1>([](std::any value) {
                    const auto cacheDuration = std::any_cast<grenade_prediction_vars::CacheDuration::ValueType>(value);
                    EXPECT_FLOAT_EQ(static_cast<float>(cacheDuration), 1.54f);
                }));
            valueSetter(1.536f);
        }));

    configSchema.performConversion(mockConfigConversion);
}

struct ConfigSchemaTrajectoryThicknessCase {
    float input;
    float expected;
};

class ConfigSchemaTrajectoryThicknessTest : public ConfigSchemaTest, public testing::WithParamInterface<ConfigSchemaTrajectoryThicknessCase> {};

TEST_P(ConfigSchemaTrajectoryThicknessTest, ClampsAndSnapsLoadedTrajectoryThickness)
{
    EXPECT_CALL(mockConfigConversion, beginRoot());
    EXPECT_CALL(mockConfigConversion, endRoot());
    EXPECT_CALL(mockConfigConversion, beginObject(testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(mockConfigConversion, endObject()).Times(testing::AnyNumber());
    EXPECT_CALL(mockConfigConversion, boolean(testing::_, testing::_, testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(mockConfigConversion, uint(testing::_, testing::_, testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(mockConfigConversion, floatValue(testing::Eq(std::u8string_view{u8"CacheDuration"}), testing::_, testing::_));

    EXPECT_CALL(mockHookContext, config()).WillOnce(testing::ReturnRef(mockConfig));
    EXPECT_CALL(mockConfig, setVariableWithoutAutoSave(ConfigVariableTypes::indexOf<grenade_prediction_vars::TrajectoryThickness>(), testing::_))
        .WillOnce(testing::WithArg<1>([this](std::any value) {
            const auto thickness = std::any_cast<grenade_prediction_vars::TrajectoryThickness::ValueType>(value);
            EXPECT_FLOAT_EQ(static_cast<float>(thickness), GetParam().expected);
        }));
    EXPECT_CALL(mockConfigConversion, floatValue(testing::Eq(std::u8string_view{u8"Thickness"}), testing::_, testing::_))
        .WillOnce(testing::WithArg<1>([this](auto valueSetter) {
            valueSetter(GetParam().input);
        }));

    configSchema.performConversion(mockConfigConversion);
}

INSTANTIATE_TEST_SUITE_P(BoundsSnappingAndNonFiniteValues, ConfigSchemaTrajectoryThicknessTest, testing::Values(
    ConfigSchemaTrajectoryThicknessCase{-1.0f, 0.5f},
    ConfigSchemaTrajectoryThicknessCase{0.01f, 0.5f},
    ConfigSchemaTrajectoryThicknessCase{1.234f, 1.23f},
    ConfigSchemaTrajectoryThicknessCase{1.235f, 1.24f},
    ConfigSchemaTrajectoryThicknessCase{2.0f, 2.0f},
    ConfigSchemaTrajectoryThicknessCase{2.996f, 3.0f},
    ConfigSchemaTrajectoryThicknessCase{3.0f, 3.0f},
    ConfigSchemaTrajectoryThicknessCase{4.0f, 3.0f},
    ConfigSchemaTrajectoryThicknessCase{std::numeric_limits<float>::quiet_NaN(), 0.5f},
    ConfigSchemaTrajectoryThicknessCase{-std::numeric_limits<float>::infinity(), 0.5f},
    ConfigSchemaTrajectoryThicknessCase{std::numeric_limits<float>::infinity(), 3.0f}));

TEST_F(ConfigSchemaTest, EachConfigVariableIsSavedOnce) {
    EXPECT_CALL(mockConfigConversion, beginRoot());
    EXPECT_CALL(mockConfigConversion, endRoot());
    EXPECT_CALL(mockConfigConversion, beginObject(testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(mockConfigConversion, endObject()).Times(testing::AnyNumber());

    EXPECT_CALL(mockHookContext, config()).WillRepeatedly(testing::ReturnRef(mockConfig));

    EXPECT_CALL(mockConfigConversion, boolean(testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::WithArg<2>([this](auto valueGetter) {
            EXPECT_CALL(mockConfig, getVariable(testing::_))
                .WillOnce(testing::WithArg<0>([this](std::size_t configVariableIndex) {
                    EXPECT_FALSE(configVariableIndexes.contains(configVariableIndex));
                    configVariableIndexes.insert(configVariableIndex);
                    return std::any{};
            }));
            valueGetter();
        }));

    EXPECT_CALL(mockConfigConversion, uint(testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::WithArg<2>([this](auto valueGetter) {
            EXPECT_CALL(mockConfig, getVariable(testing::_))
                .WillOnce(testing::WithArg<0>([this](std::size_t configVariableIndex) {
                    EXPECT_FALSE(configVariableIndexes.contains(configVariableIndex));
                    configVariableIndexes.insert(configVariableIndex);
                    return std::any{};
            }));
            valueGetter(); 
        }));
    EXPECT_CALL(mockConfigConversion, floatValue(testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::WithArg<2>([this](auto valueGetter) {
            EXPECT_CALL(mockConfig, getVariable(testing::_))
                .WillOnce(testing::WithArg<0>([this](std::size_t configVariableIndex) {
                    EXPECT_FALSE(configVariableIndexes.contains(configVariableIndex));
                    configVariableIndexes.insert(configVariableIndex);
                    return std::any{};
                }));
            valueGetter();
        }));

    configSchema.performConversion(mockConfigConversion);
    EXPECT_EQ(configVariableIndexes.size(), ConfigVariableTypes::size());
}
