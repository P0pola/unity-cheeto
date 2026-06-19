#include "pch.h"

#if USE_GFX_API != GFX_API_OPENGL
#include <d3d11.h>
#include <dxgi1_2.h>
#include "core/hooks/dx11_hook.h"

#pragma comment(lib, "dxgi.lib")

namespace {
    using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
    using Present1Fn = HRESULT(WINAPI*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
    using ResizeBuffersFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    PresentFn oPresent = nullptr;
    Present1Fn oPresent1 = nullptr;
    ResizeBuffersFn oResizeBuffers = nullptr;
}

bool DXGIHook::createDummyDevice() {
    HMODULE d3d11Module = GetModuleHandleA("d3d11.dll");
    if (!d3d11Module) {
        d3d11Module = LoadLibraryA("d3d11.dll");
        if (!d3d11Module) return false;
    }

    using PFN_D3D11CreateDevice = HRESULT(WINAPI*)(
        IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
        const D3D_FEATURE_LEVEL*, UINT, UINT,
        ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

    auto pCreateDevice = reinterpret_cast<PFN_D3D11CreateDevice>(
        GetProcAddress(d3d11Module, "D3D11CreateDevice"));
    if (!pCreateDevice) return false;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = pCreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &device, &featureLevel, &context);

    if (FAILED(hr)) return false;

    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))))
        return false;

    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(&adapter)))
        return false;

    ComPtr<IDXGIFactory2> factory;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))
        return false;

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "DXGIDummy";
    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    // Use CreateSwapChainForHwnd (flip model) — matches modern Unity
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = 100;
    sd.Height = 100;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(device.Get(), hwnd, &sd, nullptr, nullptr, &swapChain1);

    if (FAILED(hr)) {
        // Fallback: legacy bitblt model
        ComPtr<IDXGIFactory> factory1;
        adapter->GetParent(IID_PPV_ARGS(&factory1));

        DXGI_SWAP_CHAIN_DESC legacyDesc{};
        legacyDesc.BufferCount = 1;
        legacyDesc.BufferDesc.Width = 100;
        legacyDesc.BufferDesc.Height = 100;
        legacyDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        legacyDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        legacyDesc.OutputWindow = hwnd;
        legacyDesc.SampleDesc.Count = 1;
        legacyDesc.Windowed = TRUE;
        legacyDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        ComPtr<IDXGISwapChain> legacySc;
        hr = factory1->CreateSwapChain(device.Get(), &legacyDesc, &legacySc);
        if (FAILED(hr)) {
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }

        void** scVTable = *reinterpret_cast<void***>(legacySc.Get());
        presentTarget_ = scVTable[8];
        resizeTarget_ = scVTable[13];

        if (SUCCEEDED(legacySc->QueryInterface(IID_PPV_ARGS(&swapChain1)))) {
            void** sc1VTable = *reinterpret_cast<void***>(swapChain1.Get());
            present1Target_ = sc1VTable[22];
        }

        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return true;
    }

    void** sc1VTable = *reinterpret_cast<void***>(swapChain1.Get());
    presentTarget_ = sc1VTable[8];
    present1Target_ = sc1VTable[22];
    resizeTarget_ = sc1VTable[13];

    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    return true;
}

bool DXGIHook::initialize() {
    if (initialized_) return true;

    LOG_INFO("Initializing DXGI hooks...");

    if (!createDummyDevice()) {
        LOG_ERROR("Failed to create dummy device for DXGI VTable");
        return false;
    }

    auto& hm = HookManager::getInstance();

    auto presentDetour = reinterpret_cast<PresentFn>(&DXGIHook::hkPresent);
    auto resizeDetour = reinterpret_cast<ResizeBuffersFn>(&DXGIHook::hkResizeBuffers);

    if (!HookManager::install(reinterpret_cast<PresentFn>(presentTarget_), presentDetour)) {
        LOG_ERROR("DXGI Present hook failed");
        return false;
    }
    oPresent = hm.getOriginal(presentDetour);

    if (present1Target_) {
        auto present1Detour = reinterpret_cast<Present1Fn>(&DXGIHook::hkPresent1);
        if (HookManager::install(reinterpret_cast<Present1Fn>(present1Target_), present1Detour)) {
            oPresent1 = hm.getOriginal(present1Detour);
            LOG_INFO("DXGI Present1 hook installed");
        }
    }

    if (HookManager::install(reinterpret_cast<ResizeBuffersFn>(resizeTarget_), resizeDetour)) {
        oResizeBuffers = hm.getOriginal(resizeDetour);
    }

    initialized_ = true;
    LOG_INFO("DXGI hooks initialized successfully");
    return true;
}

void DXGIHook::shutdown() {
    if (!initialized_) return;
    HookManager::getInstance().shutdown();
    initialized_ = false;
}

HRESULT WINAPI DXGIHook::hkPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
    auto& self = DXGIHook::Get();

    if (self.presentCb_)
        self.presentCb_(swapChain, syncInterval, flags);

    return oPresent(swapChain, syncInterval, flags);
}

HRESULT WINAPI DXGIHook::hkPresent1(IDXGISwapChain1* swapChain, UINT syncInterval, UINT flags, const DXGI_PRESENT_PARAMETERS* pParams) {
    auto& self = DXGIHook::Get();

    if (self.presentCb_)
        self.presentCb_(swapChain, syncInterval, flags);

    return oPresent1(swapChain, syncInterval, flags, pParams);
}

HRESULT WINAPI DXGIHook::hkResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
    auto& self = DXGIHook::Get();
    if (self.resizeCb_)
        self.resizeCb_(swapChain, bufferCount, width, height, format, flags);
    return oResizeBuffers(swapChain, bufferCount, width, height, format, flags);
}

#endif // USE_GFX_API != GFX_API_OPENGL
