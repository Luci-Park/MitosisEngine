#pragma once

#include <core/ecs/ComponentAsserts.h>

#include <cstdint>

namespace mts
{
    struct ScriptRef
    {
        int32_t instanceRef = -1; ///< IScriptHost-owned handle, -1 = none
        bool started = false;     ///< OnStart has run for this instance
    };

    MTS_ASSERT_COMPONENT(ScriptRef);
}
