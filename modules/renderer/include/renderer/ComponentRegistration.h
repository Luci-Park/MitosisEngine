/**
 * @file ComponentRegistration.h
 * @author Rahul Nair
 * @brief Registers every component the renderer module defines.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

namespace mts
{
    /// Mirrors core::RegisterCoreComponents, one level up: core cannot name
    /// MeshRenderer, so the renderer registers its own components instead of
    /// folding them into the core call. App links both modules and is where
    /// the two calls sit side by side - see App::Initialize.
    void RegisterRendererComponents();
}
