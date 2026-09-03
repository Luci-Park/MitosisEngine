/**
 * @file EditorTheme.cpp
 * @author Rahul Nair
 * @brief The Slate editor theme.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include "editortheme/EditorTheme.h"

#include <imgui.h>

namespace mts
{
    namespace
    {
        constexpr ImVec4 FromHex(uint32_t hex, float alpha = 1.0f)
        {
            return ImVec4(
                static_cast<float>((hex >> 16) & 0xFF) / 255.0f,
                static_cast<float>((hex >> 8) & 0xFF) / 255.0f,
                static_cast<float>(hex & 0xFF) / 255.0f,
                alpha);
        }

        constexpr uint32_t kText = 0xE6E8EC;
        constexpr uint32_t kTextDisabled = 0x6B7280;
        constexpr uint32_t kWindow = 0x1B1E24;
        constexpr uint32_t kChild = 0x16181D;
        constexpr uint32_t kPopup = 0x1E2128;
        constexpr uint32_t kBorder = 0x2A2E37;
        constexpr uint32_t kFrame = 0x272B33;
        constexpr uint32_t kFrameHovered = 0x323742;
        constexpr uint32_t kFrameActive = 0x3A404C;
        constexpr uint32_t kTitleBg = 0x16181D;
        constexpr uint32_t kTitleBgActive = 0x1E2128;
        constexpr uint32_t kMenuBarBg = 0x1B1E24;
        constexpr uint32_t kScrollbarBg = 0x16181D;
        constexpr uint32_t kScrollbarGrab = 0x353A45;
        constexpr uint32_t kScrollbarGrabHovered = 0x424855;
        constexpr uint32_t kScrollbarGrabActive = 0x525966;
        constexpr uint32_t kButton = 0x2B303A;
        constexpr uint32_t kButtonHovered = 0x39404C;
        constexpr uint32_t kHeader = 0x2B303A;
        constexpr uint32_t kHeaderHovered = 0x39404C;
        constexpr uint32_t kTab = 0x16181D;
        constexpr uint32_t kTabHovered = 0x2B303A;
        constexpr uint32_t kTabSelected = 0x272B33;
        constexpr uint32_t kTabDimmed = 0x131519;
        constexpr uint32_t kTabDimmedSelected = 0x1E2128;
        constexpr uint32_t kAccent = 0x4C8DFF;
        constexpr uint32_t kAccentDim = 0x3665B3;

        ImGuiStyle gBaseStyle;
    }

    void EditorTheme::Apply()
    {
        ImGuiStyle &style = ImGui::GetStyle();

        style.FramePadding = ImVec2(8.0f, 5.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
        style.WindowPadding = ImVec2(10.0f, 10.0f);

        style.WindowRounding = 0.0f;
        style.ChildRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.FrameRounding = 4.0f;
        style.TabRounding = 4.0f;
        style.GrabRounding = 4.0f;
        style.ScrollbarRounding = 9.0f;

        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

        ImVec4 *colors = style.Colors;

        colors[ImGuiCol_Text] = FromHex(kText);
        colors[ImGuiCol_TextDisabled] = FromHex(kTextDisabled);

        colors[ImGuiCol_WindowBg] = FromHex(kWindow);
        colors[ImGuiCol_ChildBg] = FromHex(kChild);
        colors[ImGuiCol_PopupBg] = FromHex(kPopup);
        colors[ImGuiCol_Border] = FromHex(kBorder);

        colors[ImGuiCol_FrameBg] = FromHex(kFrame);
        colors[ImGuiCol_FrameBgHovered] = FromHex(kFrameHovered);
        colors[ImGuiCol_FrameBgActive] = FromHex(kFrameActive);

        colors[ImGuiCol_TitleBg] = FromHex(kTitleBg);
        colors[ImGuiCol_TitleBgActive] = FromHex(kTitleBgActive);
        colors[ImGuiCol_MenuBarBg] = FromHex(kMenuBarBg);

        colors[ImGuiCol_ScrollbarBg] = FromHex(kScrollbarBg);
        colors[ImGuiCol_ScrollbarGrab] = FromHex(kScrollbarGrab);
        colors[ImGuiCol_ScrollbarGrabHovered] = FromHex(kScrollbarGrabHovered);
        colors[ImGuiCol_ScrollbarGrabActive] = FromHex(kScrollbarGrabActive);

        colors[ImGuiCol_Button] = FromHex(kButton);
        colors[ImGuiCol_ButtonHovered] = FromHex(kButtonHovered);
        colors[ImGuiCol_ButtonActive] = FromHex(kAccent);

        colors[ImGuiCol_Header] = FromHex(kHeader);
        colors[ImGuiCol_HeaderHovered] = FromHex(kHeaderHovered);
        colors[ImGuiCol_HeaderActive] = FromHex(kAccent, 0.85f);

        colors[ImGuiCol_Tab] = FromHex(kTab);
        colors[ImGuiCol_TabHovered] = FromHex(kTabHovered);
#if IMGUI_VERSION_NUM >= 19100
        colors[ImGuiCol_TabSelected] = FromHex(kTabSelected);
        colors[ImGuiCol_TabDimmed] = FromHex(kTabDimmed);
        colors[ImGuiCol_TabDimmedSelected] = FromHex(kTabDimmedSelected);
#else
        colors[ImGuiCol_TabActive] = FromHex(kTabSelected);
        colors[ImGuiCol_TabUnfocused] = FromHex(kTabDimmed);
        colors[ImGuiCol_TabUnfocusedActive] = FromHex(kTabDimmedSelected);
#endif

#ifdef IMGUI_HAS_DOCK
        colors[ImGuiCol_DockingPreview] = FromHex(kAccent);
#endif

        colors[ImGuiCol_CheckMark] = FromHex(kAccent);
        colors[ImGuiCol_SliderGrab] = FromHex(kAccentDim);
        colors[ImGuiCol_SliderGrabActive] = FromHex(kAccent);
        colors[ImGuiCol_DragDropTarget] = FromHex(kAccent);

        gBaseStyle = style;
    }

    void EditorTheme::ScaleForDpi(float scale)
    {
        ImGuiStyle &style = ImGui::GetStyle();
        style = gBaseStyle;
        style.ScaleAllSizes(scale);
    }
}
