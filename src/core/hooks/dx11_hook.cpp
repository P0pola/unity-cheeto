#include "pch.h"
#include "core/hooks/dx11_hook.h"

#if USE_DX12
#pragma comment(lib, "d3d12.lib")
#else
#include <d3d11.h>
#endif
#pragma comment(lib, "dxgi.lib")

namespace {
    using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    PresentFn oPresent = nullptr;
    ResizeBuffersFn oResizeBuffers = nullptr;
}

bool DXGIHook::createDummyDevice() {
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "DXGIDummy";
    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    ComPtr<IDXGISwapChain> swapChain;

#if USE_DX12
    ComPtr<ID3D12Device> device;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> cmdQueue;
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue)))) {
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 100;
    sd.BufferDesc.Height = 100;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    if (FAILED(factory->CreateSwapChain(cmdQueue.Get(), &sd, &swapChain))) {
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }
#else
    HMODULE d3d11Module = GetModuleHandleA("d3d11.dll");
    if (!d3d11Module) {
        d3d11Module = LoadLibraryA("d3d11.dll");
        if (!d3d11Module) {
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }
    }

    using PFN_D3D11CreateDeviceAndSwapChain = HRESULT(WINAPI*)(
        IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
        const D3D_FEATURE_LEVEL*, UINT, UINT,
        const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**,
        ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

    auto pCreate = reinterpret_cast<PFN_D3D11CreateDeviceAndSwapChain>(
        GetProcAddress(d3d11Module, "D3D11CreateDeviceAndSwapChain"));
    if (!pCreate) {
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

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

    HRESULT hr = pCreate(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &swapChain, &device, nullptr, &context);

    if (FAILED(hr)) {
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }
#endif

    void** swapChainVTable = *reinterpret_cast<void***>(swapChain.Get());
    presentTarget_ = swapChainVTable[8];   // IDXGISwapChain::Present
    resizeTarget_ = swapChainVTable[13];   // IDXGISwapChain::ResizeBuffers

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

    if (HookManager::install(reinterpret_cast<ResizeBuffersFn>(resizeTarget_), resizeDetour)) {
        oResizeBuffers = hm.getOriginal(resizeDetour);
    } else {
        LOG_WARNING("DXGI ResizeBuffers hook failed");
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

HRESULT WINAPI DXGIHook::hkResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
    auto& self = DXGIHook::Get();
    if (self.resizeCb_)
        self.resizeCb_(swapChain, bufferCount, width, height, format, flags);
    return oResizeBuffers(swapChain, bufferCount, width, height, format, flags);
}
