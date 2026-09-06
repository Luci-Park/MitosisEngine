#include "script/ComponentRegistration.h"

#include "script/components/ScriptRef.h"

#include <core/ecs/ComponentRegistry.h>

namespace mir
{
    void RegisterScriptComponents()
    {
        ComponentRegistry::Instance().Register<ScriptRef>(kScriptRefFields);
    }
}
