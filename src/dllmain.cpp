#include "pch.h"

#if USE_GFX_API == GFX_API_OPENGL
#include "core/hooks/opengl_hook.h"
#include "renderer/opengl_renderer.h"
#elif USE_GFX_API == GFX_API_DX12
#include "core/hooks/dx11_hook.h"
#include "core/hooks/dx12_hook.h"
#include "renderer/dx12_renderer.h"
#else
#include "core/hooks/dx11_hook.h"
#include "renderer/dx11_renderer.h"
#endif

#include "gui/gui_manager.h"
#include "gui/icons.h"
#include "gui/widgets.h"
#include "features/fps_unlock.h"
#include "features/free_camera.h"
#include "features/world_speed.h"

static HMODULE g_hModule = nullptr;
static std::atomic<bool> g_running = true;

static ConfigVar<int> g_toggleKey{"menu.toggleKey", VK_INSERT};

void initializeFeatures() {
    Translator::GetInstance().initConfig();
    auto& gui = GuiManager::Get();

    gui.addTab(ICON_RI_PLAYER, "Player", [] {
        ImGui::TextDisabled("No player features yet");
    });

    gui.addTab(ICON_RI_WORLD, "World", [] {
        FreeCamera::Get().drawUI();
        WorldSpeed::Get().drawUI();
    });

    gui.addTab(ICON_RI_VISUAL, "Visual", [] {
        FPSUnlock::Get().drawUI();
    });

    gui.addTab(ICON_RI_MISC, "Misc", [] {

    });

    gui.addTab(ICON_RI_SETTINGS, "Setting", [] {
        ImGui::SeparatorText("Menu");
        static int toggleKeyVal = static_cast<int>(g_toggleKey);
        if (Widgets::KeyBind("Toggle Key", &toggleKeyVal)) {
            g_toggleKey = toggleKeyVal;
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Language");
        Translator::GetInstance().renderLanguageSelector();

        ImGui::Spacing();
        ImGui::SeparatorText("Config");
        if (ImGui::Button("Save"))
            ConfigManager::Instance().Save();
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            ConfigManager::Instance().GetJson().clear();
            ConfigManager::Instance().Save();
        }
    });

    gui.addTab(ICON_RI_BUG, "Debug", [] {
        ImGui::Text("Build: " __DATE__ " " __TIME__);
        ImGui::Separator();

        static int testKey = VK_F1;
        Widgets::KeyBind("Test Hotkey", &testKey);

        if (ImGui::Button("Clear Log"))
            Logger_::clear();
    });
}

static void tickToggleKey() {
    static bool wasDown = false;
    bool down = (GetAsyncKeyState(static_cast<int>(g_toggleKey)) & 0x8000) != 0;
    if (!down && wasDown)
        GuiManager::Get().toggle();
    wasDown = down;
}

#if USE_GFX_API == GFX_API_OPENGL

void onSwapBuffers(HDC hdc) {
    tickToggleKey();

    auto& renderer = OpenGLRenderer::Get();
    if (!renderer.isReady()) {
        renderer.initialize(hdc);
        return;
    }

    HotkeyManager::Get().tick();
    FPSUnlock::Get().onTick();
    FreeCamera::Get().onTick();
    WorldSpeed::Get().onTick();
    renderer.render();
}

#else // DX11 / DX12

void onPresent(IDXGISwapChain* swapChain, UINT, UINT) {
    tickToggleKey();

#if USE_GFX_API == GFX_API_DX12
    auto& renderer = DX12Renderer::Get();
    if (!renderer.isReady()) {
        static bool hooked = false;
        if (!hooked) {
            DX12Hook::Get().hookFromSwapChain(swapChain);
            hooked = true;
        }
        auto* cmdQueue = DX12Hook::Get().getCommandQueue();
        if (cmdQueue)
            renderer.initialize(swapChain, cmdQueue);
        return;
    }
#else
    auto& renderer = DX11Renderer::Get();
    if (!renderer.isReady()) {
        renderer.initialize(swapChain);
        return;
    }
#endif

    HotkeyManager::Get().tick();
    FPSUnlock::Get().onTick();
    FreeCamera::Get().onTick();
    WorldSpeed::Get().onTick();
    renderer.render();
}

void onResize(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) {
#if USE_GFX_API == GFX_API_DX12
    DX12Renderer::Get().invalidate();
#else
    DX11Renderer::Get().invalidate();
#endif
}

#endif // USE_GFX_API == GFX_API_OPENGL

DWORD WINAPI mainThread(LPVOID) {
    Logger_::attach().showTimeStamp();
    LOG_INFO("UnityCheeto loaded");

    HMODULE gameAssembly = nullptr;
    while (g_running) {
        gameAssembly = GetModuleHandleA("GameAssembly.dll");
        if (gameAssembly) break;
        gameAssembly = GetModuleHandleA("mono-2.0-bdwgc.dll");
        if (gameAssembly) break;
        Sleep(100);
    }
    if (!g_running) return 0;

    LOG_INFO("Game module found: {:p}", static_cast<void*>(gameAssembly));

    bool isMono = (GetModuleHandleA("mono-2.0-bdwgc.dll") != nullptr);
    UnityResolve::Init(gameAssembly, isMono ? UnityResolve::Mode::Mono : UnityResolve::Mode::Il2Cpp);
    UnityResolve::ThreadAttach();
    LOG_INFO("UnityResolve initialized ({})", isMono ? "Mono" : "Il2Cpp");

    initializeFeatures();

#if USE_GFX_API == GFX_API_OPENGL
    auto& hook = OpenGLHook::Get();
    hook.onSwapBuffers(onSwapBuffers);

    if (!hook.initialize()) {
        LOG_ERROR("Failed to initialize OpenGL hooks");
        return 1;
    }
#else
    auto& hook = DXGIHook::Get();
    hook.onPresent(onPresent);
    hook.onResize(onResize);

    if (!hook.initialize()) {
        LOG_ERROR("Failed to initialize DXGI hooks");
        return 1;
    }
#endif

    LOG_INFO("Initialization complete - press INSERT to open menu");

    while (g_running)
        Sleep(100);

#if USE_GFX_API == GFX_API_OPENGL
    OpenGLRenderer::Get().shutdown();
    OpenGLHook::Get().shutdown();
#elif USE_GFX_API == GFX_API_DX12
    DX12Renderer::Get().shutdown();
    DX12Hook::Get().shutdown();
    DXGIHook::Get().shutdown();
#else
    DX11Renderer::Get().shutdown();
    DXGIHook::Get().shutdown();
#endif

    Logger_::detach();

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        g_hModule = hModule;
        CreateThread(nullptr, 0, mainThread, nullptr, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running = false;
    }
    return TRUE;
}
