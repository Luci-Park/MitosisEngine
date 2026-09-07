/**
 * @file LogPanel.cpp
 * @author Rahul Nair
 * @brief Renders core::LogHistory - the editor's "Output" dock panel.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include <editor/panels/LogPanel.h>

#include <core/log/LogHistory.h>
#include <editortheme/EditorTheme.h>

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string_view>
#include <vector>

namespace mir
{
    namespace
    {
        const char *LevelLabel(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Trace:
                return "Trace";
            case LogLevel::Debug:
                return "Debug";
            case LogLevel::Info:
                return "Info";
            case LogLevel::Warn:
                return "Warn";
            case LogLevel::Error:
                return "Error";
            case LogLevel::Critical:
                return "Critical";
            case LogLevel::Off:
                return "Off";
            }
            return "";
        }

        StatusColor LevelStatusColor(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Trace:
                return StatusColor::Muted;
            case LogLevel::Debug:
                return StatusColor::Info;
            case LogLevel::Warn:
                return StatusColor::Warning;
            case LogLevel::Error:
            case LogLevel::Critical:
                return StatusColor::Danger;
            default:
                return StatusColor::Muted;
            }
        }

        bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle)
        {
            if (needle.empty())
                return true;
            const auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                                         [](unsigned char a, unsigned char b)
                                         { return std::tolower(a) == std::tolower(b); });
            return it != haystack.end();
        }
    }

    void LogPanel::Draw()
    {
        if (ImGui::Button("Clear"))
            LogHistory::Clear();

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &mAutoScroll);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        int minLevel = static_cast<int>(mMinLevel);
        if (ImGui::Combo(
                "##MinLevel", &minLevel,
                [](void *, int idx) -> const char *
                { return LevelLabel(static_cast<LogLevel>(idx)); },
                nullptr, static_cast<int>(LogLevel::Critical) + 1))
        {
            mMinLevel = static_cast<LogLevel>(minLevel);
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputTextWithHint("##Search", "Search...", mSearchBuf, sizeof(mSearchBuf));

        ImGui::Separator();

        const std::vector<LogEntry> entries = LogHistory::Snapshot();

        std::vector<std::size_t> visible;
        visible.reserve(entries.size());
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            if (entries[i].level < mMinLevel)
                continue;
            if (!ContainsCaseInsensitive(entries[i].message, mSearchBuf))
                continue;
            visible.push_back(i);
        }

        ImGui::BeginChild("##LogScroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visible.size()));
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const LogEntry &entry = entries[visible[static_cast<std::size_t>(row)]];

                if (entry.level == LogLevel::Critical)
                {
                    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
                    const ImVec2 rowMax(rowMin.x + ImGui::GetContentRegionAvail().x,
                                         rowMin.y + ImGui::GetTextLineHeightWithSpacing());
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        rowMin, rowMax,
                        ImGui::ColorConvertFloat4ToU32(EditorTheme::Color(StatusColor::Danger, 0.15f)));
                }

                if (entry.level == LogLevel::Info)
                {
                    ImGui::TextUnformatted(entry.message.c_str());
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::Color(LevelStatusColor(entry.level)));
                    ImGui::TextUnformatted(entry.message.c_str());
                    ImGui::PopStyleColor();
                }
            }
        }
        clipper.End();

        if (mAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
    }
}
