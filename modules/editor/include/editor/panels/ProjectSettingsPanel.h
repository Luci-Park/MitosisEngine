/**
 * @file ProjectSettingsPanel.h
 * @author Rahul Nair
 * @brief The "Project Settings" window's Input section - add/remove actions,
 *        add/remove/rebind bindings, save to disk.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <input/InputMap.h>
#include <window/InputSnapshot.h>

#include <filesystem>
#include <string>

namespace mir
{
    class ProjectSettingsPanel
    {
    public:
        void Draw(InputMap &inputMap, const RawInputSnapshot &raw, const std::filesystem::path &savePath);

    private:
        bool DrawAction(InputAction &action, int actionIndex, const RawInputSnapshot &raw);

        bool DrawBinding(const InputAction &action, InputBinding &binding, int actionIndex, int bindingIndex,
                          const RawInputSnapshot &raw);

        bool TryCaptureBinding(InputBinding &binding, const RawInputSnapshot &raw);

        char mNewActionName[64] = {};
        int mNewActionType = 0;

        int mListeningAction = -1;
        int mListeningBinding = -1;
        RawInputSnapshot mPreviousRaw;

        std::string mStatusMessage;
    };
}
