#pragma once

#include "C_BaseModelEntity.h"
#include <CS2/Classes/EntitySystem/CEntityHandle.h>

namespace cs2
{

struct C_CSPlayerPawn;

struct C_BaseGrenade : C_BaseModelEntity {
    using m_hThrower = CHandle<C_CSPlayerPawn>;
};

}
