#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <vector>

using Microsoft::WRL::ComPtr;

class DX12Renderer {
public:
    static DX12Renderer& Get() { static DX12Renderer inst; return inst; }

    bool initialize(IDXGISwapChain* swapChain, ID3D12CommandQueue* cmdQueue);
    void render();
    void invalidate();
    void shutdown();

    bool isReady() const { return initialized_; }
    HWND getWindow() const { return hwnd_; }

private:
    DX12Renderer() = default;

    bool createRenderTargets();
    void destroyRenderTargets();

    struct FrameContext {
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12Resource> backBuffer;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
    };

    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    ComPtr<ID3D12DescriptorHeap> srvHeap_;
    ComPtr<ID3D12GraphicsCommandList> cmdList_;
    ComPtr<ID3D12Fence> fence_;
    ComPtr<IDXGISwapChain3> swapChain_;
    ID3D12CommandQueue* cmdQueue_ = nullptr;

    std::vector<FrameContext> frames_;
    HANDLE fenceEvent_ = nullptr;
    UINT64 fenceValue_ = 0;
    UINT bufferCount_ = 0;

    HWND hwnd_ = nullptr;
    WNDPROC originalWndProc_ = nullptr;
    bool initialized_ = false;

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
