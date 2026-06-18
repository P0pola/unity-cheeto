#include "pch.h"
#include "core/hooks/dx12_hook.h"
#include "renderer/dx12_renderer.h"
#include "gui/gui_manager.h"
#include "gui/icons.h"
#include "features/fps_unlock.h"
#include "features/free_camera.h"

static HMODULE g_hModule = nullptr;
static std::atomic<bool> g_running = true;

void initializeFeatures() {
    auto& gui = GuiManager::Get();

    gui.addTab(ICON_RI_PLAYER, "Player", [] {
        ImGui::TextDisabled("No player features yet");
    });

    gui.addTab(ICON_RI_WORLD, "World", [] {
        FreeCamera::Get().drawUI();
    });

    gui.addTab(ICON_RI_VISUAL, "Visual", [] {
        FPSUnlock::Get().drawUI();
    });

    gui.addTab(ICON_RI_MISC, "Misc", [] {
        ImGui::TextDisabled("No misc features yet");
    });

    gui.addTab(ICON_RI_SETTINGS, "Setting", [] {
        ImGui::SeparatorText("Menu");
        ImGui::Text("Toggle: INSERT");

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
        if (ImGui::Button("Clear Log"))
            Logger_::clear();
    });
}

void onPresent(IDXGISwapChain* swapChain, UINT, UINT) {
    auto& renderer = DX12Renderer::Get();
    if (!renderer.isReady()) {
        auto* cmdQueue = DX12Hook::Get().getCommandQueue();
        if (cmdQueue)
            renderer.initialize(swapChain, cmdQueue);
        return;
    }

    HotkeyManager::Get().tick();
    FPSUnlock::Get().onTick();
    FreeCamera::Get().onTick();

    renderer.render();
}

void onResize(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) {
    DX12Renderer::Get().invalidate();
}

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

    HotkeyManager::Get().onRelease(VK_INSERT, [] { GuiManager::Get().toggle(); });

    initializeFeatures();

    auto& dx12 = DX12Hook::Get();
    dx12.onPresent(onPresent);
    dx12.onResize(onResize);

    if (!dx12.initialize()) {
        LOG_ERROR("Failed to initialize DX12 hooks");
        return 1;
    }

    LOG_INFO("Initialization complete - press INSERT to open menu");

    while (g_running)
        Sleep(100);

    DX12Renderer::Get().shutdown();
    DX12Hook::Get().shutdown();
    Logger_::detach();

    FreeLibraryAndExitThread(g_hModule, 0);
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
