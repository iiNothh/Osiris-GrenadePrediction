#pragma once

#include <Platform/Macros/IsPlatform.h>

struct GrenadePredictionPlatformCapabilities {
    static constexpr bool supportsLiveProjectilePrediction{IS_WIN64()};
};
