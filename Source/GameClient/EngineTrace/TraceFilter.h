#pragma once

#include <CS2/Constants/CollisionGroup.h>
#include <CS2/Constants/InteractionLayers.h>
#include <CS2/Constants/PhysicsQueryFlag.h>
#include <Utils/Optional.h>

namespace engine_trace {
    struct TraceFilterExcludedEntities {
        constexpr TraceFilterExcludedEntities(void* first = nullptr, void* second = nullptr) noexcept
        {
            if (first == second)
                second = nullptr;
            this->first = first;
            this->second = second;
        }

        void* first{};
        void* second{};
    };

    struct TraceFilterParameters {
        cs2::engine_trace::InteractionLayer interactsWith{};
        cs2::CollisionGroup collisionGroup{};
        cs2::PhysicsQueryFlag queryFlags{};
        Optional<cs2::engine_trace::InteractionLayer> interactsExclude{};
        Optional<cs2::engine_trace::InteractionLayer> interactsAs{};
    };
}
