#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <functional>

using Microsoft::WRL::ComPtr;

class DX12Hook {
public:
    static DX12Hook& Get() { static DX12Hook inst; return inst; }

    // Called from onPresent when DX12 is detected — hooks ExecuteCommandLists to capture command queue
    bool hookFromSwapChain(IDXGISwapChain* swapChain);
    void shutdown();

    ID3D12CommandQueue* getCommandQueue() const { return commandQueue_; }

private:
    DX12Hook() = default;

    static void WINAPI hkExecuteCommandLists(ID3D12CommandQueue* queue, UINT numLists, ID3D12CommandList* const* lists);

    ID3D12CommandQueue* commandQueue_ = nullptr;
    void* executeTarget_ = nullptr;
    bool initialized_ = false;
};
