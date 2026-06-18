#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <functional>

using Microsoft::WRL::ComPtr;

class DX12Hook {
public:
    static DX12Hook& Get() { static DX12Hook inst; return inst; }

    using PresentCallback = std::function<void(IDXGISwapChain*, UINT, UINT)>;
    using ResizeCallback = std::function<void(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT)>;

    bool initialize();
    void shutdown();

    void onPresent(PresentCallback cb) { presentCb_ = std::move(cb); }
    void onResize(ResizeCallback cb) { resizeCb_ = std::move(cb); }

    ID3D12Device* getDevice() const { return device_.Get(); }
    ID3D12CommandQueue* getCommandQueue() const { return commandQueue_; }

private:
    DX12Hook() = default;

    static HRESULT WINAPI hkPresent(IDXGISwapChain3* swapChain, UINT syncInterval, UINT flags);
    static HRESULT WINAPI hkResizeBuffers(IDXGISwapChain3* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT format, UINT flags);
    static void WINAPI hkExecuteCommandLists(ID3D12CommandQueue* queue, UINT numLists, ID3D12CommandList* const* lists);

    bool hookPresent();
    bool hookExecuteCommandLists();
    bool createDummyDevice();

    ComPtr<ID3D12Device> device_;
    ID3D12CommandQueue* commandQueue_ = nullptr;

    PresentCallback presentCb_;
    ResizeCallback resizeCb_;

    void* presentTarget_ = nullptr;
    void* executeTarget_ = nullptr;
    void* resizeTarget_ = nullptr;

    bool initialized_ = false;
    bool presentHooked_ = false;
};
