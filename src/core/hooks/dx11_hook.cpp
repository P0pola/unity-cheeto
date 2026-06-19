#include "pch.h"
#include "core/hooks/dx11_hook.h"

#pragma comment(lib, "dxgi.lib")

namespace {
    using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    PresentFn oPresent = nullptr;
    ResizeBuffersFn oResizeBuffers = nullptr;
}

bool DX11Hook::createDummyDevice() {
    HMODULE d3d11Module = GetModuleHandleA("d3d11.dll");
    if (!d3d11Module) {
        d3d11Module = LoadLibraryA("d3d11.dll");
        if (!d3d11Module) return false;
    }

    using PFN_D3D11CreateDeviceAndSwapChain = HRESULT(WINAPI*)(
        IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
        const D3D_FEATURE_LEVEL*, UINT, UINT,
        const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**,
        ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

    auto pCreate = reinterpret_cast<PFN_D3D11CreateDeviceAndSwapChain>(
        GetProcAddress(d3d11Module, "D3D11CreateDeviceAndSwapChain"));
    if (!pCreate) return false;

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "DXGIDummy";
    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 100;
    sd.BufferDesc.Height = 100;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;

    HRESULT hr = pCreate(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &swapChain, &device, nullptr, &context);

    if (FAILED(hr)) {
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    void** swapChainVTable = *reinterpret_cast<void***>(swapChain.Get());
    presentTarget_ = swapChainVTable[8];   // IDXGISwapChain::Present
    resizeTarget_ = swapChainVTable[13];   // IDXGISwapChain::ResizeBuffers

    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    return true;
}

bool DX11Hook::initialize() {
    if (initialized_) return true;

    LOG_INFO("Initializing DX11 hooks...");

    if (!createDummyDevice()) {
        LOG_ERROR("Failed to create dummy device for DX11 VTable");
        return false;
    }

    auto& hm = HookManager::getInstance();

    auto presentDetour = reinterpret_cast<PresentFn>(&DX11Hook::hkPresent);
    auto resizeDetour = reinterpret_cast<ResizeBuffersFn>(&DX11Hook::hkResizeBuffers);

    if (!HookManager::install(reinterpret_cast<PresentFn>(presentTarget_), presentDetour)) {
        LOG_ERROR("DX11 Present hook failed");
        return false;
    }
    oPresent = hm.getOriginal(presentDetour);

    if (HookManager::install(reinterpret_cast<ResizeBuffersFn>(resizeTarget_), resizeDetour)) {
        oResizeBuffers = hm.getOriginal(resizeDetour);
    } else {
        LOG_WARNING("DX11 ResizeBuffers hook failed");
    }

    initialized_ = true;
    LOG_INFO("DX11 hooks initialized successfully");
    return true;
}

void DX11Hook::shutdown() {
    if (!initialized_) return;
    HookManager::getInstance().shutdown();
    initialized_ = false;
}

HRESULT WINAPI DX11Hook::hkPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
    auto& self = DX11Hook::Get();

    if (self.presentCb_)
        self.presentCb_(swapChain, syncInterval, flags);

    return oPresent(swapChain, syncInterval, flags);
}

HRESULT WINAPI DX11Hook::hkResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
    auto& self = DX11Hook::Get();
    if (self.resizeCb_)
        self.resizeCb_(swapChain, bufferCount, width, height, format, flags);
    return oResizeBuffers(swapChain, bufferCount, width, height, format, flags);
}
