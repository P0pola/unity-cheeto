#include "pch.h"
#include "core/hooks/dx11_hook.h"
#include "core/hooks/dx12_hook.h"
#include "renderer/dx11_renderer.h"
#include "renderer/dx12_renderer.h"
#include "gui/gui_manager.h"
#include "gui/icons.h"
#include "gui/widgets.h"
#include "features/fps_unlock.h"
#include "features/free_camera.h"
#include "features/world_speed.h"

static HMODULE g_hModule = nullptr;
static std::atomic<bool> g_running = true;

enum class GraphicsAPI { Unknown, DX11, DX12 };
static GraphicsAPI g_api = GraphicsAPI::Unknown;

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

void onPresent(IDXGISwapChain* swapChain, UINT, UINT) {
    // Dynamic menu toggle hotkey
    {
        static bool wasDown = false;
        bool down = (GetAsyncKeyState(static_cast<int>(g_toggleKey)) & 0x8000) != 0;
        if (!down && wasDown)
            GuiManager::Get().toggle();
        wasDown = down;
    }

    // First call: detect graphics API via QueryInterface
    if (g_api == GraphicsAPI::Unknown) {
        ComPtr<ID3D12Device> d3d12Device;
        if (SUCCEEDED(swapChain->GetDevice(IID_PPV_ARGS(&d3d12Device)))) {
            g_api = GraphicsAPI::DX12;
            LOG_INFO("Detected D3D12 device");

            // For DX12 we also need ExecuteCommandLists hook to capture command queue
            DX12Hook::Get().hookFromSwapChain(swapChain);
        } else {
            g_api = GraphicsAPI::DX11;
            LOG_INFO("Detected D3D11 device");
        }
        return;
    }

    if (g_api == GraphicsAPI::DX12) {
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
        WorldSpeed::Get().onTick();
        renderer.render();
    } else {
        auto& renderer = DX11Renderer::Get();
        if (!renderer.isReady()) {
            renderer.initialize(swapChain);
            return;
        }

        HotkeyManager::Get().tick();
        FPSUnlock::Get().onTick();
        FreeCamera::Get().onTick();
        WorldSpeed::Get().onTick();
        renderer.render();
    }
}

void onResize(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) {
    if (g_api == GraphicsAPI::DX12)
        DX12Renderer::Get().invalidate();
    else if (g_api == GraphicsAPI::DX11)
        DX11Renderer::Get().invalidate();
}

DWORD WINAPI mainThread(LPVOID) {
    Logger_::attach().showTimeStamp();
    LOG_INFO("UnityCheeto loaded");
    //printf("11111111");
   // return 1;
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

    // Hook DXGI Present via DX11 dummy device (works for both DX11 and DX12 games)
    auto& hook = DX11Hook::Get();
    hook.onPresent(onPresent);
    hook.onResize(onResize);

    if (!hook.initialize()) {
        LOG_ERROR("Failed to initialize DXGI hooks");
        return 1;
    }

    LOG_INFO("Initialization complete - press INSERT to open menu");

    while (g_running)
        Sleep(100);

    if (g_api == GraphicsAPI::DX12) {
        DX12Renderer::Get().shutdown();
        DX12Hook::Get().shutdown();
    } else if (g_api == GraphicsAPI::DX11) {
        DX11Renderer::Get().shutdown();
       
    }
    DX11Hook::Get().shutdown();
  
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
