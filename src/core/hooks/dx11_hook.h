#pragma once
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <functional>

using Microsoft::WRL::ComPtr;

class DXGIHook {
public:
    static DXGIHook& Get() { static DXGIHook inst; return inst; }

    using PresentCallback = std::function<void(IDXGISwapChain*, UINT, UINT)>;
    using ResizeCallback = std::function<void(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT)>;

    bool initialize();
    void shutdown();

    void onPresent(PresentCallback cb) { presentCb_ = std::move(cb); }
    void onResize(ResizeCallback cb) { resizeCb_ = std::move(cb); }

private:
    DXGIHook() = default;

    static HRESULT WINAPI hkPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
    static HRESULT WINAPI hkPresent1(IDXGISwapChain1* swapChain, UINT syncInterval, UINT flags, const DXGI_PRESENT_PARAMETERS* pParams);
    static HRESULT WINAPI hkResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT format, UINT flags);

    bool createDummyDevice();

    PresentCallback presentCb_;
    ResizeCallback resizeCb_;

    void* presentTarget_ = nullptr;
    void* present1Target_ = nullptr;
    void* resizeTarget_ = nullptr;

    bool initialized_ = false;
};
