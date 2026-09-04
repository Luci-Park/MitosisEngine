#include <editor/Editor.h>

#include <core/fs/Paths.h>
#include <core/log/Log.h>
#include <editortheme/EditorTheme.h>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

#include <algorithm>

namespace mts
{
    Editor::~Editor()
    {
        // Shutdown needs a VulkanRenderer& it doesn't have here, so a caller
        // that skips the explicit Shutdown() leaks the ImGui context rather
        // than crash into a dangling device - same tradeoff App makes for
        // its own owned subsystems.
    }

    bool Editor::Initialize(Window &window, VulkanRenderer &renderer)
    {
        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        mImGuiIniPath = (ExecutableDir() / "imgui.ini").string();
        io.IniFilename = mImGuiIniPath.c_str();

        ImFontConfig fontConfig;
        fontConfig.OversampleH = 3;
        const std::string fontPath = FontPath("Inter.ttf").string();
        if (io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f, &fontConfig) == nullptr)
        {
            MTS_LOG_ERROR("Failed to load font: {}", fontPath);
            io.Fonts->AddFontDefault();
        }

        static const ImWchar iconRanges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
        ImFontConfig iconConfig;
        iconConfig.MergeMode = true;
        iconConfig.PixelSnapH = true;
        iconConfig.GlyphMinAdvanceX = 16.0f;
        const std::string iconFontPath = FontPath(FONT_ICON_FILE_NAME_FAS).string();
        if (io.Fonts->AddFontFromFileTTF(iconFontPath.c_str(), 16.0f, &iconConfig, iconRanges) == nullptr)
            MTS_LOG_ERROR("Failed to load icon font: {}", iconFontPath);

        EditorTheme::Apply();
        EditorTheme::ScaleForDpi(window.ContentScale());

        void *nativeHandle = window.NativeHandleForImGui();
        if (nativeHandle == nullptr)
        {
            MTS_LOG_ERROR("Window backend has no native handle for ImGui");
            ImGui::DestroyContext();
            return false;
        }

        if (!ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow *>(nativeHandle), true))
        {
            MTS_LOG_ERROR("ImGui_ImplGlfw_InitForVulkan failed");
            ImGui::DestroyContext();
            return false;
        }

        if (!renderer.InitImGuiVulkanBackend())
        {
            MTS_LOG_ERROR("ImGui Vulkan backend initialization failed");
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            return false;
        }

        mInitialized = true;
        return true;
    }

    void Editor::Shutdown(VulkanRenderer &renderer)
    {
        if (!mInitialized)
            return;

        renderer.ShutdownImGuiVulkanBackend();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        mInitialized = false;
    }

    void Editor::BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    SceneMenuAction Editor::DrawLayout(bool enableLayout, bool showDemoWindow)
    {
        SceneMenuAction sceneAction = SceneMenuAction::None;

        if (enableLayout)
        {
            const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(
                0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

            ImGuiDockNode *dockspaceNode = ImGui::DockBuilderGetNode(dockspaceId);
            if (dockspaceNode != nullptr && dockspaceNode->IsEmpty())
            {
                ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

                ImGuiID center = dockspaceId;
                const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.2f, nullptr, &center);
                const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);
                const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, nullptr, &center);

                ImGui::DockBuilderDockWindow("Hierarchy", left);
                ImGui::DockBuilderDockWindow("Inspector", right);
                ImGui::DockBuilderDockWindow("Output", bottom);

                ImGui::DockBuilderFinish(dockspaceId);
            }

            for (const char *name : {"Hierarchy", "Inspector", "Output"})
            {
                ImGui::Begin(name);
                ImGui::End();
            }

            // ImGuiDockNodeFlags_PassthruCentralNode leaves the dockspace's
            // center undocked, so the 3D scene shows through there rather
            // than in any named window - that central node's rect, not a
            // window titled "Scene", is what the scene pass must clip to.
            if (const ImGuiDockNode *centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId);
                centralNode != nullptr)
            {
                mSceneViewportRect = VkRect2D{
                    .offset{static_cast<int32_t>(centralNode->Pos.x), static_cast<int32_t>(centralNode->Pos.y)},
                    .extent{static_cast<uint32_t>(std::max(centralNode->Size.x, 0.0f)),
                            static_cast<uint32_t>(std::max(centralNode->Size.y, 0.0f))}};
            }

            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("New Scene"))
                        sceneAction = SceneMenuAction::New;
                    if (ImGui::MenuItem("Save Scene"))
                        sceneAction = SceneMenuAction::Save;
                    if (ImGui::MenuItem("Load Scene"))
                        sceneAction = SceneMenuAction::Load;
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Debug"))
                {
                    ImGui::MenuItem("Style Editor", nullptr, &mShowStyleEditor);
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }

            if (mShowStyleEditor)
            {
                if (ImGui::Begin("Style Editor", &mShowStyleEditor))
                    ImGui::ShowStyleEditor();
                ImGui::End();
            }
        }
        else
        {
            // No dockspace to derive a viewport from - fall back to the
            // full swapchain, same as before a layout is ever drawn.
            mSceneViewportRect = VkRect2D{};
        }

        if (showDemoWindow)
            ImGui::ShowDemoWindow();

        return sceneAction;
    }

    ImDrawData *Editor::EndFrame()
    {
        ImGui::Render();
        return ImGui::GetDrawData();
    }
}
