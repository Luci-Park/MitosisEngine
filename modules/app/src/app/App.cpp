#include <app/App.h>

#include <core/ecs/ComponentRegistry.h>
#include <core/ecs/DeferredAccess.h>
#include <core/ecs/TransformHierarchy.h>
#include <core/fs/Paths.h>
#include <core/log/Log.h>
#include <editortheme/EditorTheme.h>
#include <renderer/ComponentRegistration.h>
#include <renderer/RenderSystem.h>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

#include <algorithm>
#include <chrono>
#include <cstring>

namespace mts
{
    App::~App()
    {
        Shutdown();
    }

    bool App::Initialize(const AppDesc &desc)
    {
        mDesc = desc;

        WindowDesc windowDesc{};
        windowDesc.mWidth = desc.mWidth;
        windowDesc.mHeight = desc.mHeight;
        windowDesc.mTitle = desc.mTitle;

        mWindow = Window::Create(windowDesc);
        if (!mWindow)
        {
            MTS_LOG_ERROR("Window creation failed");
            return false;
        }

        if (!mRenderer.Initialize({.window = mWindow.get(),
                                   .appName = desc.mAppName,
                                   .enableValidation = desc.mEnableValidation}))
        {
            MTS_LOG_ERROR("Renderer initialization failed");
            mWindow.reset();
            return false;
        }

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
        EditorTheme::ScaleForDpi(mWindow->ContentScale());

        void *nativeHandle = mWindow->NativeHandleForImGui();
        if (nativeHandle == nullptr)
        {
            MTS_LOG_ERROR("Window backend has no native handle for ImGui");
            ImGui::DestroyContext();
            mRenderer.Shutdown();
            mWindow.reset();
            return false;
        }

        if (!ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow *>(nativeHandle), true))
        {
            MTS_LOG_ERROR("ImGui_ImplGlfw_InitForVulkan failed");
            ImGui::DestroyContext();
            mRenderer.Shutdown();
            mWindow.reset();
            return false;
        }

        if (!mRenderer.InitImGuiVulkanBackend())
        {
            MTS_LOG_ERROR("ImGui Vulkan backend initialization failed");
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            mRenderer.Shutdown();
            mWindow.reset();
            return false;
        }

        mImGuiInitialized = true;
        mInitialized = true;

        // scene graph + install destroy hook
        InstallHierarchy(mWorld);

        RegisterCoreComponents();
        RegisterRendererComponents();

        // Publishes the frame's buffer so a caller with only a World - a script
        // binding, an editor command - can defer a structural change it is not
        // allowed to make immediately. The scheduler already flushes this
        // buffer at every phase boundary; a second one would be flushed by
        // nobody, which is why the resource holds a pointer.
        mWorld.EmplaceResource<FrameCommands>(FrameCommands{&mCommands});

        // should be before any other system in PostUpdate
        mScheduler.Add<TransformPropagateSystem>(SystemPhase::PostUpdate);

        // Render runs after PostUpdate, so every WorldTransform it reads is
        // already current for this frame - see RenderSystem's own comment.
        mScheduler.Add<RenderSystem>(SystemPhase::Render, mRenderer);

        return true;
    }

    AssetCache *App::Assets()
    {
        if (mAssetCache.has_value())
            return &*mAssetCache;

        if (mAssetLoadFailed)
            return nullptr; // already tried and logged; do not re-stat the disk every call

        const std::filesystem::path manifestPath = CookedAssetsDir() / "manifest.blob";
        mAssetManifest = AssetManifest::LoadFile(manifestPath);
        if (!mAssetManifest.has_value())
        {
            MTS_LOG_ERROR("Asset manifest load failed, assets unavailable: {}", manifestPath.string());
            mAssetLoadFailed = true;
            return nullptr;
        }

        // after the manifest is engaged, never before: the cache stores a raw
        // pointer to it
        mAssetCache.emplace(&*mAssetManifest, CookedAssetsDir());
        return &*mAssetCache;
    }

    SystemContext App::MakeContext(float dt)
    {
        return SystemContext{mWorld, mCommands, dt, mElapsed, mFrame};
    }

    void App::DrawEditorUI()
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

        // ImGuiDockNodeFlags_PassthruCentralNode leaves the dockspace's center undocked
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

    void App::Run()
    {
        if (!mInitialized)
        {
            return;
        }

        SystemContext startContext = MakeContext(0.0f);
        mScheduler.Start(startContext);

        auto previous = std::chrono::steady_clock::now();

        while (!mWindow->ShouldClose())
        {
            mWindow->PollEvents();

            const auto now = std::chrono::steady_clock::now();
            const float dt = std::min(std::chrono::duration<float>(now - previous).count(),
                                      mDesc.mMaxDeltaSeconds);
            previous = now;
            mElapsed += dt;

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // NewFrame/Render must run every iteration regardless (ImGui's frame
            // state requires the pair), but building window content for a frame
            // DrawFrame is about to discard wastes the CPU work, not the pairing.
            if (mWindow->Width() != 0 && mWindow->Height() != 0)
            {
                if (mDesc.mEnableEditorLayout)
                    DrawEditorUI();

                if (mDesc.mShowImGuiDemo)
                    ImGui::ShowDemoWindow();
            }

            ImGui::Render();

            // RenderSystem calls VulkanRenderer::DrawFrame from inside
            // Update (SystemPhase::Render), so this frame's draw data has to
            // be handed to the renderer before Update runs, not after.
            mRenderer.SetImGuiDrawData(ImGui::GetDrawData());
            mRenderer.SetSceneViewport(mSceneViewportRect);

            SystemContext context = MakeContext(dt);
            mScheduler.Update(context);

            ++mFrame;
        }
    }

    void App::Shutdown()
    {
        if (!mInitialized)
        {
            return;
        }

        // let all the systems stop first
        SystemContext stopContext = MakeContext(0.0f);
        mScheduler.Stop(stopContext);

        // Dropped, not kept: Initialize may run again (see below), and it
        // registers TransformPropagateSystem unconditionally. Keeping the old
        // list would run a second copy of it, and of every game system, on
        // every frame of the next session.
        mScheduler.Reset();

        // Cache before manifest: the cache points at the manifest, and Initialize
        // may be called again afterwards. Leaving the cache engaged over a
        // destroyed manifest would leave a dangling pointer behind.
        mAssetCache.reset();
        mAssetManifest.reset();
        mAssetLoadFailed = false;

        if (mImGuiInitialized)
        {
            // Reverse of Initialize: Vulkan backend needs mDevice still alive,
            // so it goes before mRenderer.Shutdown(); the GLFW backend needs
            // mWindow still alive, so it goes before mWindow.reset().
            mRenderer.ShutdownImGuiVulkanBackend();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            mImGuiInitialized = false;
        }

        mRenderer.Shutdown();
        // Renderer holds the surface built from the window: window dies last.
        mWindow.reset();
        mInitialized = false;
        MTS_LOG_INFO("App shut down");
    }
}
