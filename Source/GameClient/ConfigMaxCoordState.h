#pragma once

#include <GameClient/DLLs/Tier0Dll.h>
#include <Utils/Optional.h>

struct ConfigMaxCoordState {
    explicit ConfigMaxCoordState(Tier0Dll tier0Dll) noexcept
        : ConfigMaxCoordState{tier0Dll.configMaxCoordPointer()}
    {
    }

    explicit ConfigMaxCoordState(float* configMaxCoord) noexcept
        : configMaxCoord{configMaxCoord}
    {
    }

    [[nodiscard]] Optional<float> current() const noexcept
    {
        if (configMaxCoord)
            return *configMaxCoord;
        return {};
    }

    float* configMaxCoord;
};
