#include <cstdint>
#include <optional>
#include <type_traits>

#include <gtest/gtest.h>

#include <CS2/Classes/EntitySystem/CEntityClass.h>
#include <CS2/Classes/EntitySystem/CEntityIdentity.h>
#include <CS2/Classes/Entities/C_BaseModelEntity.h>
#include <CS2/Classes/Entities/C_CSPlayerPawn.h>
#include <CS2/Classes/ConVarTypes.h>
#include <Features/Visuals/GrenadePrediction/GrenadePlayerCollisionSnapshotBuilder.h>
#include <GameClient/Entities/TeamNumber.h>
#include <Utils/Optional.h>

namespace
{

struct SnapshotBuilderPlayer {
    cs2::C_BaseModelEntity entity{};
    cs2::CEntityIdentity identity{};
    Optional<TeamNumber> team{};
    Optional<cs2::Vector> origin{};
    Optional<cs2::Vector> mins{};
    Optional<cs2::Vector> maxs{};

    void set(cs2::CEntityClass* entityClass, std::uint32_t handle, TeamNumber newTeam, cs2::Vector newOrigin,
        cs2::Vector newMins = {}, cs2::Vector newMaxs = {1.0f, 1.0f, 1.0f}) noexcept
    {
        entity.identity = &identity;
        identity.entity = &entity;
        identity.entityClass = entityClass;
        identity.handle = cs2::CEntityHandle{handle};
        team = newTeam;
        origin = newOrigin;
        mins = newMins;
        maxs = newMaxs;
    }
};

struct SnapshotBuilderEntityClassifier {
    const cs2::CEntityClass* playerClass{};

    template <typename Entity>
    [[nodiscard]] bool entityIs(const cs2::CEntityClass* entityClass) const noexcept
    {
        static_assert(std::is_same_v<Entity, cs2::C_CSPlayerPawn>);
        return entityClass == playerClass;
    }
};

struct SnapshotBuilderCvarSystem {
    std::optional<bool> teammatesAreEnemies{false};

    template <typename ConVar>
    [[nodiscard]] std::optional<typename ConVar::ValueType> getConVarValue() const noexcept
    {
        static_assert(std::is_same_v<ConVar, cs2::mp_teammates_are_enemies>);
        return teammatesAreEnemies;
    }
};

struct GrenadePlayerCollisionSnapshotBuilderTestContext;

struct SnapshotBuilderBaseEntity {
    GrenadePlayerCollisionSnapshotBuilderTestContext& context;
    cs2::C_BaseEntity* entity;

    [[nodiscard]] Optional<TeamNumber> optionalTeamNumber() const noexcept;
    [[nodiscard]] Optional<cs2::Vector> absOrigin() const noexcept;
};

struct SnapshotBuilderCollisionProperty {
    GrenadePlayerCollisionSnapshotBuilderTestContext& context;
    cs2::C_BaseEntity* entity;

    [[nodiscard]] Optional<cs2::Vector> mins() const noexcept;
    [[nodiscard]] Optional<cs2::Vector> maxs() const noexcept;
};

struct SnapshotBuilderBaseModelEntity {
    GrenadePlayerCollisionSnapshotBuilderTestContext& context;
    cs2::C_BaseEntity* entity;

    [[nodiscard]] SnapshotBuilderCollisionProperty collisionProperty() const noexcept { return {context, entity}; }
};

struct GrenadePlayerCollisionSnapshotBuilderTestContext {
    [[nodiscard]] SnapshotBuilderEntityClassifier& entityClassifier() noexcept { return classifier; }
    [[nodiscard]] const SnapshotBuilderCvarSystem& cvarSystem() const noexcept { return cvars; }

    [[nodiscard]] const SnapshotBuilderPlayer& playerFor(const cs2::C_BaseEntity* entity) const noexcept
    {
        for (const auto* const player : players) {
            if (player && static_cast<const cs2::C_BaseEntity*>(&player->entity) == entity)
                return *player;
        }
        return *players[0];
    }

    template <template <typename> typename T>
    [[nodiscard]] decltype(auto) make(cs2::C_BaseEntity* entity) noexcept;

    cs2::CEntityClass playerClass{};
    cs2::CEntityClass otherClass{};
    SnapshotBuilderEntityClassifier classifier{&playerClass};
    SnapshotBuilderCvarSystem cvars;
    SnapshotBuilderPlayer* players[3]{};
};

Optional<TeamNumber> SnapshotBuilderBaseEntity::optionalTeamNumber() const noexcept
{
    return context.playerFor(entity).team;
}

Optional<cs2::Vector> SnapshotBuilderBaseEntity::absOrigin() const noexcept
{
    return context.playerFor(entity).origin;
}

Optional<cs2::Vector> SnapshotBuilderCollisionProperty::mins() const noexcept
{
    return context.playerFor(entity).mins;
}

Optional<cs2::Vector> SnapshotBuilderCollisionProperty::maxs() const noexcept
{
    return context.playerFor(entity).maxs;
}

template <template <typename> typename T>
decltype(auto) GrenadePlayerCollisionSnapshotBuilderTestContext::make(cs2::C_BaseEntity* entity) noexcept
{
    if constexpr (std::is_same_v<T<GrenadePlayerCollisionSnapshotBuilderTestContext>, BaseEntity<GrenadePlayerCollisionSnapshotBuilderTestContext>>) {
        return SnapshotBuilderBaseEntity{*this, entity};
    } else {
        static_assert(std::is_same_v<T<GrenadePlayerCollisionSnapshotBuilderTestContext>, BaseModelEntity<GrenadePlayerCollisionSnapshotBuilderTestContext>>);
        return SnapshotBuilderBaseModelEntity{*this, entity};
    }
}

using SnapshotBuilder = GrenadePlayerCollisionSnapshotBuilder<GrenadePlayerCollisionSnapshotBuilderTestContext>;

void registerPlayers(GrenadePlayerCollisionSnapshotBuilderTestContext& context, SnapshotBuilderPlayer& localPlayer,
    SnapshotBuilderPlayer& firstCandidate, SnapshotBuilderPlayer* secondCandidate = nullptr) noexcept
{
    context.players[0] = &localPlayer;
    context.players[1] = &firstCandidate;
    context.players[2] = secondCandidate;
}

void observe(SnapshotBuilder& builder, GrenadePlayerCollisionCollectionScratch& scratch, const SnapshotBuilderPlayer& player) noexcept
{
    builder.observe(scratch, player.identity);
}

TEST(GrenadePlayerCollisionSnapshotBuilderTest, RejectsInvalidIdentityAndPlayerData)
{
    GrenadePlayerCollisionSnapshotBuilderTestContext context;
    SnapshotBuilderPlayer localPlayer;
    SnapshotBuilderPlayer candidate;
    localPlayer.set(&context.playerClass, 1, TeamNumber::CT, {});
    candidate.set(&context.playerClass, 2, TeamNumber::TT, {});
    registerPlayers(context, localPlayer, candidate);
    SnapshotBuilder builder{context};
    GrenadePlayerCollisionCollectionScratch scratch;
    GrenadePlayerCollisionSnapshot snapshot{.count = 1, .status = GrenadePlayerCollisionSnapshotStatus::Available, .revision = 7};
    cs2::CEntityIdentity invalidIdentity{};
    invalidIdentity.entityClass = &context.playerClass;
    invalidIdentity.handle = cs2::CEntityHandle{2};

    builder.begin(scratch);
    builder.observe(scratch, invalidIdentity);
    builder.finish(snapshot, scratch, &localPlayer.entity);

    EXPECT_TRUE(scratch.playerDataInvalid);
    EXPECT_EQ(snapshot.status, GrenadePlayerCollisionSnapshotStatus::Unavailable);
    EXPECT_EQ(snapshot.count, 0);
    EXPECT_EQ(snapshot.revision, 8);

    snapshot = {.count = 1, .status = GrenadePlayerCollisionSnapshotStatus::Available, .revision = 9};
    candidate.team = {};
    builder.begin(scratch);
    observe(builder, scratch, candidate);
    builder.finish(snapshot, scratch, &localPlayer.entity);

    EXPECT_TRUE(scratch.playerDataInvalid);
    EXPECT_EQ(snapshot.status, GrenadePlayerCollisionSnapshotStatus::Unavailable);
    EXPECT_EQ(snapshot.count, 0);
    EXPECT_EQ(snapshot.revision, 10);
}

TEST(GrenadePlayerCollisionSnapshotBuilderTest, RejectsOverflowedCollection)
{
    GrenadePlayerCollisionSnapshotBuilderTestContext context;
    SnapshotBuilderPlayer localPlayer;
    SnapshotBuilderPlayer candidate;
    localPlayer.set(&context.playerClass, 1, TeamNumber::CT, {});
    candidate.set(&context.playerClass, 2, TeamNumber::TT, {});
    registerPlayers(context, localPlayer, candidate);
    SnapshotBuilder builder{context};
    GrenadePlayerCollisionCollectionScratch scratch;
    GrenadePlayerCollisionSnapshot snapshot{.count = 1, .status = GrenadePlayerCollisionSnapshotStatus::Available, .revision = 4};

    builder.begin(scratch);
    for (std::uint32_t handle = 2; handle < static_cast<std::uint32_t>(GrenadePlayerCollisionCollectionScratch::kCapacity) + 3; ++handle) {
        candidate.identity.handle = cs2::CEntityHandle{handle};
        observe(builder, scratch, candidate);
    }
    builder.finish(snapshot, scratch, &localPlayer.entity);

    EXPECT_EQ(scratch.count, GrenadePlayerCollisionCollectionScratch::kCapacity);
    EXPECT_TRUE(scratch.overflowed);
    EXPECT_EQ(snapshot.status, GrenadePlayerCollisionSnapshotStatus::Unavailable);
    EXPECT_EQ(snapshot.count, 0);
    EXPECT_EQ(snapshot.revision, 5);
}

TEST(GrenadePlayerCollisionSnapshotBuilderTest, RejectsInvalidLocalPawn)
{
    GrenadePlayerCollisionSnapshotBuilderTestContext context;
    SnapshotBuilderPlayer localPlayer;
    SnapshotBuilderPlayer candidate;
    localPlayer.set(&context.otherClass, 1, TeamNumber::CT, {});
    candidate.set(&context.playerClass, 2, TeamNumber::TT, {});
    registerPlayers(context, localPlayer, candidate);
    SnapshotBuilder builder{context};
    GrenadePlayerCollisionCollectionScratch scratch;
    GrenadePlayerCollisionSnapshot snapshot{.count = 1, .status = GrenadePlayerCollisionSnapshotStatus::Available, .revision = 2};

    builder.begin(scratch);
    observe(builder, scratch, candidate);
    builder.finish(snapshot, scratch, &localPlayer.entity);

    EXPECT_EQ(snapshot.status, GrenadePlayerCollisionSnapshotStatus::Unavailable);
    EXPECT_EQ(snapshot.count, 0);
    EXPECT_EQ(snapshot.revision, 3);
}

TEST(GrenadePlayerCollisionSnapshotBuilderTest, AppliesTeammateEligibilityPolicy)
{
    GrenadePlayerCollisionSnapshotBuilderTestContext context;
    SnapshotBuilderPlayer localPlayer;
    SnapshotBuilderPlayer teammate;
    SnapshotBuilderPlayer enemy;
    localPlayer.set(&context.playerClass, 1, TeamNumber::CT, {});
    teammate.set(&context.playerClass, 2, TeamNumber::CT, {2.0f, 0.0f, 0.0f});
    enemy.set(&context.playerClass, 3, TeamNumber::TT, {3.0f, 0.0f, 0.0f});
    registerPlayers(context, localPlayer, teammate, &enemy);
    SnapshotBuilder builder{context};
    GrenadePlayerCollisionCollectionScratch scratch;
    GrenadePlayerCollisionSnapshot snapshot;

    builder.begin(scratch);
    observe(builder, scratch, teammate);
    observe(builder, scratch, enemy);
    builder.finish(snapshot, scratch, &localPlayer.entity);

    ASSERT_EQ(snapshot.count, 2);
    EXPECT_FALSE(snapshot.candidates[0].relationshipEligible);
    EXPECT_TRUE(snapshot.candidates[1].relationshipEligible);
}

TEST(GrenadePlayerCollisionSnapshotBuilderTest, SortsAndDeduplicatesByRawHandleKeepingFirstDuplicate)
{
    GrenadePlayerCollisionSnapshotBuilderTestContext context;
    SnapshotBuilderPlayer localPlayer;
    SnapshotBuilderPlayer candidate;
    localPlayer.set(&context.playerClass, 1, TeamNumber::CT, {});
    candidate.set(&context.playerClass, 4, TeamNumber::TT, {4.0f, 0.0f, 0.0f});
    registerPlayers(context, localPlayer, candidate);
    SnapshotBuilder builder{context};
    GrenadePlayerCollisionCollectionScratch scratch;
    GrenadePlayerCollisionSnapshot snapshot;

    builder.begin(scratch);
    observe(builder, scratch, candidate);
    candidate.identity.handle = cs2::CEntityHandle{2};
    candidate.origin = cs2::Vector{2.0f, 0.0f, 0.0f};
    observe(builder, scratch, candidate);
    candidate.identity.handle = cs2::CEntityHandle{4};
    candidate.origin = cs2::Vector{40.0f, 0.0f, 0.0f};
    observe(builder, scratch, candidate);
    builder.finish(snapshot, scratch, &localPlayer.entity);

    ASSERT_EQ(snapshot.count, 2);
    EXPECT_EQ(snapshot.candidates[0].rawHandle, 2);
    EXPECT_EQ(snapshot.candidates[0].mins.x, 2.0f);
    EXPECT_EQ(snapshot.candidates[1].rawHandle, 4);
    EXPECT_EQ(snapshot.candidates[1].mins.x, 4.0f);
}

TEST(GrenadePlayerCollisionSnapshotBuilderTest, PreservesRevisionForIdenticalData)
{
    GrenadePlayerCollisionSnapshotBuilderTestContext context;
    SnapshotBuilderPlayer localPlayer;
    SnapshotBuilderPlayer candidate;
    localPlayer.set(&context.playerClass, 1, TeamNumber::CT, {});
    candidate.set(&context.playerClass, 2, TeamNumber::TT, {2.0f, 0.0f, 0.0f});
    registerPlayers(context, localPlayer, candidate);
    SnapshotBuilder builder{context};
    GrenadePlayerCollisionCollectionScratch scratch;
    GrenadePlayerCollisionSnapshot snapshot;

    builder.begin(scratch);
    observe(builder, scratch, candidate);
    builder.finish(snapshot, scratch, &localPlayer.entity);
    const auto revision = snapshot.revision;

    builder.begin(scratch);
    observe(builder, scratch, candidate);
    builder.finish(snapshot, scratch, &localPlayer.entity);

    EXPECT_EQ(snapshot.revision, revision);
}

TEST(GrenadePlayerCollisionSnapshotBuilderTest, UpdatesRevisionForVisibleSnapshotChanges)
{
    GrenadePlayerCollisionSnapshotBuilderTestContext context;
    SnapshotBuilderPlayer localPlayer;
    SnapshotBuilderPlayer teammate;
    localPlayer.set(&context.playerClass, 1, TeamNumber::CT, {});
    teammate.set(&context.playerClass, 2, TeamNumber::CT, {2.0f, 0.0f, 0.0f});
    registerPlayers(context, localPlayer, teammate);
    SnapshotBuilder builder{context};
    GrenadePlayerCollisionCollectionScratch scratch;
    GrenadePlayerCollisionSnapshot snapshot;

    builder.begin(scratch);
    observe(builder, scratch, teammate);
    builder.finish(snapshot, scratch, &localPlayer.entity);
    const auto revision = snapshot.revision;
    ASSERT_EQ(snapshot.count, 1);
    EXPECT_FALSE(snapshot.candidates[0].relationshipEligible);

    context.cvars.teammatesAreEnemies = true;
    builder.begin(scratch);
    observe(builder, scratch, teammate);
    builder.finish(snapshot, scratch, &localPlayer.entity);

    EXPECT_EQ(snapshot.revision, revision + 1);
    ASSERT_EQ(snapshot.count, 1);
    EXPECT_TRUE(snapshot.candidates[0].relationshipEligible);
}

}
