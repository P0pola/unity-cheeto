#include "pch.h"
#include "core/hooks/dx12_hook.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace {
    using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain3*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(WINAPI*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    using ExecuteCommandListsFn = void(WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

    PresentFn oPresent = nullptr;
    ResizeBuffersFn oResizeBuffers = nullptr;
    ExecuteCommandListsFn oExecuteCommandLists = nullptr;
}

bool DX12Hook::createDummyDevice() {
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return false;

    ComPtr<IDXGIAdapter> adapter;
    factory->EnumAdapters(0, &adapter);

    if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_))))
        return false;

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ComPtr<ID3D12CommandQueue> cmdQueue;
    if (FAILED(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue))))
        return false;

    DXGI_SWAP_CHAIN_DESC1 swapDesc{};
    swapDesc.BufferCount = 2;
    swapDesc.Width = 100;
    swapDesc.Height = 100;
    swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.SampleDesc.Count = 1;

    HWND hwnd = CreateWindowExA(0, "STATIC", "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);

    ComPtr<IDXGISwapChain1> swapChain1;
    if (FAILED(factory->CreateSwapChainForHwnd(cmdQueue.Get(), hwnd, &swapDesc, nullptr, nullptr, &swapChain1))) {
        DestroyWindow(hwnd);
        return false;
    }

    ComPtr<IDXGISwapChain3> swapChain3;
    swapChain1.As(&swapChain3);

    void** swapChainVTable = *reinterpret_cast<void***>(swapChain3.Get());
    void** cmdQueueVTable = *reinterpret_cast<void***>(cmdQueue.Get());

    presentTarget_ = swapChainVTable[8];   // Present
    resizeTarget_ = swapChainVTable[13];   // ResizeBuffers
    executeTarget_ = cmdQueueVTable[10];   // ExecuteCommandLists

    DestroyWindow(hwnd);
    device_.Reset();

    return true;
}

bool DX12Hook::hookPresent() {
    auto& hm = HookManager::getInstance();

    auto presentDetour = reinterpret_cast<PresentFn>(&DX12Hook::hkPresent);
    auto resizeDetour = reinterpret_cast<ResizeBuffersFn>(&DX12Hook::hkResizeBuffers);

    if (!HookManager::install(reinterpret_cast<PresentFn>(presentTarget_), presentDetour)) {
        LOG_ERROR("Present hook failed");
        return false;
    }
    oPresent = hm.getOriginal(presentDetour);

    if (HookManager::install(reinterpret_cast<ResizeBuffersFn>(resizeTarget_), resizeDetour)) {
        oResizeBuffers = hm.getOriginal(resizeDetour);
    } else {
        LOG_WARNING("ResizeBuffers hook failed");
    }

    presentHooked_ = true;
    return true;
}

bool DX12Hook::hookExecuteCommandLists() {
    auto& hm = HookManager::getInstance();
    auto execDetour = reinterpret_cast<ExecuteCommandListsFn>(&DX12Hook::hkExecuteCommandLists);

    if (!HookManager::install(reinterpret_cast<ExecuteCommandListsFn>(executeTarget_), execDetour)) {
        LOG_ERROR("ExecuteCommandLists hook failed");
        return false;
    }
    oExecuteCommandLists = hm.getOriginal(execDetour);
    return true;
}

bool DX12Hook::initialize() {
    if (initialized_) return true;

    LOG_INFO("Initializing DX12 hooks...");

    if (!createDummyDevice()) {
        LOG_ERROR("Failed to create dummy DX12 device for VTable");
        return false;
    }

    if (!hookPresent()) {
        LOG_WARNING("Present hook failed, trying ExecuteCommandLists...");
        if (!hookExecuteCommandLists()) {
            LOG_ERROR("All DX12 hook methods failed");
            return false;
        }
    } else {
        hookExecuteCommandLists();
    }

    initialized_ = true;
    LOG_INFO("DX12 hooks initialized successfully");
    return true;
}

void DX12Hook::shutdown() {
    if (!initialized_) return;
    HookManager::getInstance().shutdown();
    initialized_ = false;
}

HRESULT WINAPI DX12Hook::hkPresent(IDXGISwapChain3* swapChain, UINT syncInterval, UINT flags) {
    auto& self = DX12Hook::Get();

    if (!self.commandQueue_) {
        ComPtr<ID3D12Device> dev;
        if (SUCCEEDED(swapChain->GetDevice(IID_PPV_ARGS(&dev)))) {
            self.device_ = dev;
        }
    }

    if (self.presentCb_)
        self.presentCb_(swapChain, syncInterval, flags);

    return oPresent(swapChain, syncInterval, flags);
}

HRESULT WINAPI DX12Hook::hkResizeBuffers(IDXGISwapChain3* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
    auto& self = DX12Hook::Get();
    if (self.resizeCb_)
        self.resizeCb_(swapChain, bufferCount, width, height, format, flags);
    return oResizeBuffers(swapChain, bufferCount, width, height, format, flags);
}

void WINAPI DX12Hook::hkExecuteCommandLists(ID3D12CommandQueue* queue, UINT numLists, ID3D12CommandList* const* lists) {
    auto& self = DX12Hook::Get();
    if (!self.commandQueue_) {
        self.commandQueue_ = queue;
        LOG_INFO("CommandQueue captured: {:p}", static_cast<void*>(queue));
    }
    oExecuteCommandLists(queue, numLists, lists);
}
