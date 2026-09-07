/**
 * @file EditorTheme.h
 * @author Rahul Nair
 * @brief The Slate editor theme.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

struct ImVec4;

namespace mir
{
    enum class StatusColor
    {
        Muted,
        Info,
        Warning,
        Danger,
    };

    class EditorTheme
    {
    public:
        EditorTheme() = delete;

        static void Apply();
        static void ScaleForDpi(float scale);
        static ImVec4 Color(StatusColor status, float alpha = 1.0f);
    };
}
