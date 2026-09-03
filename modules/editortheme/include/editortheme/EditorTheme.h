/**
 * @file EditorTheme.h
 * @author Rahul Nair
 * @brief The Slate editor theme.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

namespace mts
{
    class EditorTheme
    {
    public:
        EditorTheme() = delete;

        static void Apply();
        static void ScaleForDpi(float scale);
    };
}
