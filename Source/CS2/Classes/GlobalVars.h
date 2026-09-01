#pragma once

#include <cstdint>

#include <Utils/Pad.h>

namespace cs2
{

struct GlobalVars {
    PAD(48); // FIXME: get offset to curtime dynamically
    float curtime;
    using frametime = float;
    using tickcount = std::int32_t;
};

}
