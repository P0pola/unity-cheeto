#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class DX11Renderer {
public:
    static DX11Renderer& Get() { static DX11Renderer inst; return inst; }

    bool initialize(IDXGISwapChain* swapChain);
    void render();
    void invalidate();
    void shutdown();

    bool isReady() const { return initialized_; }
    HWND getWindow() const { return hwnd_; }

private:
    DX11Renderer() = default;

    bool createRenderTarget();
    void destroyRenderTarget();

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    IDXGISwapChain* swapChain_ = nullptr;

    HWND hwnd_ = nullptr;
    WNDPROC originalWndProc_ = nullptr;
    bool initialized_ = false;

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
