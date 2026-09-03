#include <app/App.h>

#include <core/ecs/ComponentRegistry.h>
#include <core/ecs/DeferredAccess.h>
#include <core/ecs/TransformHierarchy.h>
#include <core/fs/Paths.h>
#include <core/log/Log.h>
#include <editortheme/EditorTheme.h>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

#include <algorithm>
#include <chrono>

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

        if (!ImGui_ImplGlfw_InitForVulkan(
                static_cast<GLFWwindow *>(mWindow->NativeHandleForImGui()), true))
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

        // Destruction needs no system: InstallHierarchy puts the scene graph
        // in place and arms the destroy hook, so World::DestroyEntity cascades
        // and nothing is ever left orphaned for a pass to reap. Called here so
        // a world destroyed into before its first AddTransform still cascades.
        InstallHierarchy(mWorld);

        // Before anything may load a script: the registry gives a name to
        // whoever claims it first, and a script-declared component that stole
        // "Transform" would be refused here rather than at its own callsite.
        RegisterCoreComponents();

        // Publishes the frame's buffer so a caller with only a World - a script
        // binding, an editor command - can defer a structural change it is not
        // allowed to make immediately. The scheduler already flushes this
        // buffer at every phase boundary; a second one would be flushed by
        // nobody, which is why the resource holds a pointer.
        mWorld.EmplaceResource<FrameCommands>(FrameCommands{&mCommands});

        // should be before any other system in PostUpdate
        mScheduler.Add<TransformPropagateSystem>(SystemPhase::PostUpdate);

        mInitialized = true;
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

    void App::CollectDrawInstances()
    {
        if (mDrawQuery == nullptr)
            mDrawQuery = &mWorld.GetOrCreateQuery<const WorldTransform>(With<TriangleRenderer>{});

        mDrawInstances.clear();

        mDrawQuery->ForEach([this](Entity, const WorldTransform &world)
                            { mDrawInstances.push_back(world.Matrix()); });
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

            SystemContext context = MakeContext(dt);
            mScheduler.Update(context);

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

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

                ImGui::DockBuilderDockWindow("Scene", left);
                ImGui::DockBuilderDockWindow("Inspector", right);
                ImGui::DockBuilderDockWindow("Output", bottom);

                ImGui::DockBuilderFinish(dockspaceId);
            }

            ImGui::Begin("Scene");
            ImGui::End();

            ImGui::Begin("Inspector");
            ImGui::End();

            ImGui::Begin("Output");
            ImGui::End();

            if (mDesc.mShowImGuiDemo)
                ImGui::ShowDemoWindow();

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

            ImGui::Render();

            CollectDrawInstances();
            mRenderer.DrawFrame(mDrawInstances, ImGui::GetDrawData());

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
        mDrawQuery = nullptr;
        mDrawInstances.clear();

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
