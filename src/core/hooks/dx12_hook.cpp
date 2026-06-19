#include "pch.h"

#if USE_DX12
#include "core/hooks/dx12_hook.h"

#pragma comment(lib, "dxgi.lib")

namespace {
    using ExecuteCommandListsFn = void(WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
    ExecuteCommandListsFn oExecuteCommandLists = nullptr;
}

bool DX12Hook::hookFromSwapChain(IDXGISwapChain* swapChain) {
    if (initialized_) return true;

    ComPtr<ID3D12Device> device;
    if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&device))))
        return false;

    // Create a temporary command queue to get the VTable
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ComPtr<ID3D12CommandQueue> tmpQueue;
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&tmpQueue))))
        return false;

    void** cmdQueueVTable = *reinterpret_cast<void***>(tmpQueue.Get());
    executeTarget_ = cmdQueueVTable[10]; // ExecuteCommandLists

    auto& hm = HookManager::getInstance();
    auto execDetour = reinterpret_cast<ExecuteCommandListsFn>(&DX12Hook::hkExecuteCommandLists);

    if (!HookManager::install(reinterpret_cast<ExecuteCommandListsFn>(executeTarget_), execDetour)) {
        LOG_ERROR("ExecuteCommandLists hook failed");
        return false;
    }
    oExecuteCommandLists = hm.getOriginal(execDetour);

    initialized_ = true;
    LOG_INFO("DX12 ExecuteCommandLists hook installed");
    return true;
}

void DX12Hook::shutdown() {
    if (!initialized_) return;
    HookManager::getInstance().shutdown();
    initialized_ = false;
}

void WINAPI DX12Hook::hkExecuteCommandLists(ID3D12CommandQueue* queue, UINT numLists, ID3D12CommandList* const* lists) {
    auto& self = DX12Hook::Get();
    if (!self.commandQueue_) {
        self.commandQueue_ = queue;
        LOG_INFO("CommandQueue captured: {:p}", static_cast<void*>(queue));
    }
    oExecuteCommandLists(queue, numLists, lists);
}

#endif // USE_DX12
