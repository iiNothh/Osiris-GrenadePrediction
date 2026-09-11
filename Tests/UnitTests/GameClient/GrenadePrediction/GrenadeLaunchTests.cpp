#include <bit>
#include <cstdint>
#include <initializer_list>
#include <type_traits>

#include <gtest/gtest.h>

#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Classes/Entities/WeaponEntities.h>
#include <CS2/Classes/EntitySystem/CEntityIdentity.h>
#include <CS2/Classes/Vector.h>
#include <CS2/Constants/EntityHandle.h>
#include <GameClient/GrenadePrediction/GrenadeLaunch.h>
#include <MemoryPatterns/PatternTypes/ClientPatternTypes.h>
#include <MemoryPatterns/PatternTypes/EntityPatternTypes.h>

namespace
{

constexpr cs2::CEntityHandle kPawnHandle{1};
constexpr cs2::CEntityHandle kOtherHandle{2};
constexpr cs2::CEntityHandle kInvalidHandle{cs2::INVALID_EHANDLE_INDEX};

enum class NativeOutput {
    Untouched,
    NonFiniteOrigin,
    NonFiniteVelocity,
    Finite
};

struct NativeCallRecorder {
    int calls{};
    NativeOutput output{NativeOutput::Untouched};
    cs2::C_CSWeaponBase* grenade{};
    cs2::C_CSPlayerPawn* pawn{};
    bool finalArgument{};
};

NativeCallRecorder* activeRecorder{};

class ActiveRecorderGuard {
public:
    explicit ActiveRecorderGuard(NativeCallRecorder& recorder) noexcept
        : recorder{recorder}
    {
        activeRecorder = &recorder;
    }

    ~ActiveRecorderGuard()
    {
        if (activeRecorder == &recorder)
            activeRecorder = nullptr;
    }

    ActiveRecorderGuard(const ActiveRecorderGuard&) = delete;
    ActiveRecorderGuard& operator=(const ActiveRecorderGuard&) = delete;

private:
    NativeCallRecorder& recorder;
};

char buildGrenadeLaunch(cs2::C_CSWeaponBase* grenade, cs2::C_CSPlayerPawn* pawn, cs2::Vector* origin, cs2::Vector* velocity, bool finalArgument) noexcept
{
    if (activeRecorder == nullptr)
        return {};
    ++activeRecorder->calls;
    activeRecorder->grenade = grenade;
    activeRecorder->pawn = pawn;
    activeRecorder->finalArgument = finalArgument;
    switch (activeRecorder->output) {
    case NativeOutput::Untouched: break;
    case NativeOutput::NonFiniteOrigin:
        *origin = {std::bit_cast<float>(std::uint32_t{0x7FC00000u}), 2.0f, 3.0f};
        *velocity = {4.0f, 5.0f, 6.0f};
        break;
    case NativeOutput::NonFiniteVelocity:
        *origin = {1.0f, 2.0f, 3.0f};
        *velocity = {4.0f, std::bit_cast<float>(std::uint32_t{0x7F800000u}), 6.0f};
        break;
    case NativeOutput::Finite:
        *origin = {1.0f, 2.0f, 3.0f};
        *velocity = {4.0f, 5.0f, 6.0f};
        break;
    }
    return {};
}

struct GrenadeStorage {
    cs2::C_BaseCSGrenade grenade{};
    cs2::CEntityHandle owner{};
};

struct GrenadeLaunchTestContext {
    struct PatternSearchResults {
        template <typename T>
        [[nodiscard]] auto get() const noexcept
        {
            if constexpr (std::is_same_v<T, BuildGrenadeLaunchFunction>)
                return context.buildLaunch;
            else {
                static_assert(std::is_same_v<T, OffsetToOwnerEntity>);
                return EntityOffset<cs2::C_BaseEntity::m_hOwnerEntity, std::int32_t>{context.ownerOffset};
            }
        }

        GrenadeLaunchTestContext& context;
    };

    GrenadeLaunchTestContext() noexcept
        : results{*this}
    {
    }

    [[nodiscard]] const PatternSearchResults& patternSearchResults() const noexcept
    {
        return results;
    }

    cs2::C_CSWeaponBase::BuildGrenadeLaunch* buildLaunch{};
    std::int32_t ownerOffset{};
    PatternSearchResults results;
};

class GrenadeLaunchTest : public testing::Test {
protected:
    void makeNativeLaunchAvailable() noexcept
    {
        context.buildLaunch = &buildGrenadeLaunch;
        context.ownerOffset = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(&grenade.owner) - reinterpret_cast<std::uintptr_t>(&grenade.grenade));
        grenade.owner = kPawnHandle;
        pawn.identity = &identity;
    }

    NativeCallRecorder recorder;
    ActiveRecorderGuard activeRecorderGuard{recorder};
    GrenadeLaunchTestContext context;
    GrenadeStorage grenade;
    cs2::CEntityIdentity identity{.handle = kPawnHandle};
    cs2::C_CSPlayerPawn pawn{};
};

TEST_F(GrenadeLaunchTest, RejectsNullInputsAndPawnWithoutIdentity)
{
    GrenadeLaunch launch{context};

    EXPECT_FALSE(launch.get(nullptr, &pawn).hasValue());
    EXPECT_FALSE(launch.get(&grenade.grenade, nullptr).hasValue());
    EXPECT_FALSE(launch.get(&grenade.grenade, &pawn).hasValue());
    EXPECT_EQ(recorder.calls, 0);
}

TEST_F(GrenadeLaunchTest, RejectsMissingCallableOrOwnerResult)
{
    makeNativeLaunchAvailable();
    GrenadeLaunch launch{context};

    context.buildLaunch = nullptr;
    EXPECT_FALSE(launch.get(&grenade.grenade, &pawn).hasValue());

    context.buildLaunch = &buildGrenadeLaunch;
    context.ownerOffset = {};
    EXPECT_FALSE(launch.get(&grenade.grenade, &pawn).hasValue());
    EXPECT_EQ(recorder.calls, 0);
}

TEST_F(GrenadeLaunchTest, RejectsInvalidAndMismatchedOwnerHandles)
{
    makeNativeLaunchAvailable();
    GrenadeLaunch launch{context};

    grenade.owner = kInvalidHandle;
    EXPECT_FALSE(launch.get(&grenade.grenade, &pawn).hasValue());

    grenade.owner = kPawnHandle;
    identity.handle = kInvalidHandle;
    EXPECT_FALSE(launch.get(&grenade.grenade, &pawn).hasValue());

    identity.handle = kOtherHandle;
    EXPECT_FALSE(launch.get(&grenade.grenade, &pawn).hasValue());
    EXPECT_EQ(recorder.calls, 0);
}

TEST_F(GrenadeLaunchTest, RejectsUntouchedAndNonFiniteNativeOutputs)
{
    makeNativeLaunchAvailable();
    GrenadeLaunch launch{context};

    for (const auto output : {NativeOutput::Untouched, NativeOutput::NonFiniteOrigin, NativeOutput::NonFiniteVelocity}) {
        recorder.output = output;
        EXPECT_FALSE(launch.get(&grenade.grenade, &pawn).hasValue());
    }
    EXPECT_EQ(recorder.calls, 3);
}

TEST_F(GrenadeLaunchTest, ReturnsFiniteNativeOutput)
{
    makeNativeLaunchAvailable();
    recorder.output = NativeOutput::Finite;
    GrenadeLaunch launch{context};

    const auto result = launch.get(&grenade.grenade, &pawn);

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().origin, (cs2::Vector{1.0f, 2.0f, 3.0f}));
    EXPECT_EQ(result.value().velocity, (cs2::Vector{4.0f, 5.0f, 6.0f}));
    EXPECT_EQ(recorder.calls, 1);
    EXPECT_EQ(recorder.grenade, static_cast<cs2::C_CSWeaponBase*>(&grenade.grenade));
    EXPECT_EQ(recorder.pawn, &pawn);
    EXPECT_FALSE(recorder.finalArgument);
}

}
