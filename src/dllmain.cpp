#include "pch.h"
#include "app.h"

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

static HMODULE g_hModule = nullptr;
static std::atomic<bool> g_running = true;

// ============================================================================
// Hook callbacks
// ============================================================================

#if USE_GFX_API == GFX_API_OPENGL

static void onSwapBuffers(HDC hdc) {
    auto& renderer = OpenGLRenderer::Get();
    if (!renderer.isReady()) {
        renderer.initialize(hdc);
        return;
    }
    App::tick();
    renderer.render();
}

#else

static void onPresent(IDXGISwapChain* swapChain, UINT, UINT) {
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
    App::tick();
    renderer.render();
}

static void onResize(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) {
#if USE_GFX_API == GFX_API_DX12
    DX12Renderer::Get().invalidate();
#else
    DX11Renderer::Get().invalidate();
#endif
}

#endif

// ============================================================================
// Main thread
// ============================================================================

static DWORD WINAPI mainThread(LPVOID) {
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

    App::initialize();

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
