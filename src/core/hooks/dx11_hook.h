#pragma once
#include <dxgi.h>
#include <wrl/client.h>
#include <functional>

using Microsoft::WRL::ComPtr;

class DX11Hook {
public:
    static DX11Hook& Get() { static DX11Hook inst; return inst; }

    using PresentCallback = std::function<void(IDXGISwapChain*, UINT, UINT)>;
    using ResizeCallback = std::function<void(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT)>;

    bool initialize();
    void shutdown();

    void onPresent(PresentCallback cb) { presentCb_ = std::move(cb); }
    void onResize(ResizeCallback cb) { resizeCb_ = std::move(cb); }

private:
    DX11Hook() = default;

    static HRESULT WINAPI hkPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
    static HRESULT WINAPI hkResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT format, UINT flags);

    bool createDummyDevice();

    PresentCallback presentCb_;
    ResizeCallback resizeCb_;

    void* presentTarget_ = nullptr;
    void* resizeTarget_ = nullptr;

    bool initialized_ = false;
};
