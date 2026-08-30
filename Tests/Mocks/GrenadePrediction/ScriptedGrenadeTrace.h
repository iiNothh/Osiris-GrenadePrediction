#pragma once

#include <optional>
#include <type_traits>

#include <CS2/Classes/ConVarTypes.h>
#include <Features/Visuals/GrenadePrediction/GrenadeSimulator.h>

struct ScriptedGrenadeTrace {
    static constexpr int kCapacity = 64;
    Optional<TraceResult> results[kCapacity]{};
    int resultCount{};
    int calls{};
    Optional<TraceResult> fallback{};
    void* lastExcludedFirst{};
    void* lastExcludedSecond{};
    std::uint64_t lastMask{};
    std::uint8_t lastCollisionGroup{};
    std::uint8_t lastQueryByte{};
    cs2::Vector lastStart{};
    cs2::Vector lastEnd{};

    void push(Optional<TraceResult> result) noexcept
    {
        if (resultCount < kCapacity)
            results[resultCount++] = result;
    }
    void clearAfterScript() noexcept { fallback = TraceResult{1.0f, {}, {}}; }
    [[nodiscard]] Optional<TraceResult> traceGrenadeHull(cs2::Vector start, cs2::Vector end, void*) noexcept
    {
        lastStart = start;
        lastEnd = end;
        const auto index = calls++;
        return index < resultCount ? results[index] : fallback;
    }
    [[nodiscard]] Optional<TraceResult> traceGrenadeHull(cs2::Vector start, cs2::Vector end, void* skipEntity,
        std::uint64_t mask, std::uint8_t group, std::uint8_t query) noexcept
    {
        lastMask = mask;
        lastCollisionGroup = group;
        lastQueryByte = query;
        return traceGrenadeHull(start, end, skipEntity);
    }
    [[nodiscard]] Optional<TraceResult> traceGrenadeHull(cs2::Vector start, cs2::Vector end,
        engine_trace::TraceFilterExcludedEntities excludedEntities, std::uint64_t mask, std::uint8_t group, std::uint8_t query) noexcept
    {
        lastExcludedFirst = excludedEntities.first;
        lastExcludedSecond = excludedEntities.second;
        return traceGrenadeHull(start, end, excludedEntities.first, mask, group, query);
    }
};

struct ScriptedGrenadeCvarSystem {
    std::optional<float> gravity;
    template <typename ConVar>
    [[nodiscard]] std::optional<typename ConVar::ValueType> getConVarValue() noexcept
    {
        static_assert(std::is_same_v<ConVar, cs2::sv_gravity>);
        return gravity;
    }
};

struct ScriptedGrenadeEntitySystem {
    cs2::CEntityInstance entity{};
    cs2::CEntityIdentity identity{};
    cs2::CEntityClass dynamicPropClass{};
    cs2::CEntityClass otherClass{};
    void setDynamicProp(cs2::CEntityHandle handle) noexcept { set(handle, dynamicPropClass); }
    void setNonDynamicProp(cs2::CEntityHandle handle) noexcept { set(handle, otherClass); }
    [[nodiscard]] cs2::CEntityInstance* getEntityFromHandle(cs2::CEntityHandle handle) const noexcept { return identity.handle == handle ? identity.entity : nullptr; }
private:
    void set(cs2::CEntityHandle handle, cs2::CEntityClass& entityClass) noexcept
    {
        entity.identity = &identity;
        identity.entity = &entity;
        identity.entityClass = &entityClass;
        identity.handle = handle;
    }
};

struct ScriptedEntityClassifier {
    const cs2::CEntityClass* dynamicPropClass{};
    template <typename EntityType>
    [[nodiscard]] bool entityIs(const cs2::CEntityClass* entityClass) const noexcept
    {
        static_assert(std::is_same_v<EntityType, cs2::C_DynamicProp>);
        return entityClass == dynamicPropClass;
    }
};

struct GrenadeSimulatorTestHookContext {
    ScriptedGrenadeTrace trace;
    ScriptedGrenadeEntitySystem entitySystem;
    ScriptedEntityClassifier classifier{&entitySystem.dynamicPropClass};
    [[nodiscard]] ScriptedEntityClassifier& entityClassifier() noexcept { return classifier; }
    template <template <typename> typename T>
    [[nodiscard]] decltype(auto) make() noexcept
    {
        if constexpr (std::is_same_v<T<GrenadeSimulatorTestHookContext>, EngineTrace<GrenadeSimulatorTestHookContext>>)
            return (trace);
        else
            return (entitySystem);
    }
};

template <typename HookContext>
struct GrenadeSimulatorTestAccess {
    using Simulator = GrenadeSimulator<HookContext>;
    [[nodiscard]] static StepResult step(Simulator& simulator, cs2::Vector& position, cs2::Vector& velocity, cs2::GrenadeKind kind,
        void* skipEntity = nullptr, float gravity = grenade_prediction_params::kDefaultServerGravity) noexcept
    {
        return simulator.step(position, velocity, kind, skipEntity, gravity);
    }
    [[nodiscard]] static bool shouldDetonate(cs2::GrenadeKind kind, int tick) noexcept { return Simulator::shouldDetonate(kind, tick); }
    [[nodiscard]] static auto applyContactResponse(Simulator& simulator, const TraceResult& trace, cs2::Vector& velocity, cs2::GrenadeKind kind) noexcept
    {
        return simulator.applyContactResponse(trace, velocity, kind);
    }
    [[nodiscard]] static StepResult movementSubstep(Simulator& simulator, cs2::Vector& position, cs2::Vector& velocity, cs2::GrenadeKind kind,
        void* skipEntity = nullptr, float gravity = grenade_prediction_params::kDefaultServerGravity) noexcept
    {
        typename Simulator::SimulationScratch scratch{nullptr, nullptr};
        StepResult result;
        const auto collision = simulator.movementSubstep(scratch, position, velocity, kind, skipEntity, result, gravity);
        result.traceSucceeded = collision.traceSucceeded;
        result.impactDetonate = collision.impactDetonate;
        return result;
    }
};
