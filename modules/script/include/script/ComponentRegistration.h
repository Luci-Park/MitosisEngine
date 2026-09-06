/**
 * @file ComponentRegistration.h
 * @author Sumin Park
 * @brief Registers every component the script module defines.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

namespace mir
{
    /// Mirrors core::RegisterCoreComponents / RegisterRendererComponents - core
    /// cannot name ScriptRef, so script registers its own. Must run before any
    /// scene is loaded (SceneIO looks ScriptRef up by name through the
    /// registry), same ordering requirement as the other two calls - see
    /// App::Initialize.
    void RegisterScriptComponents();
}
