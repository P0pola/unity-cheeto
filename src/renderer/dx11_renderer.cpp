#include "pch.h"
#include "renderer/dx11_renderer.h"
#include "renderer/imgui_style.h"
#include "gui/gui_manager.h"
#include "gui/remixicon.hpp"
#include "gui/LXGWWenKai-Regular.hpp"

#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool DX11Renderer::initialize(IDXGISwapChain* swapChain) {
    if (initialized_) return true;

    swapChain_ = swapChain;

    if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(device_.GetAddressOf()))))
        return false;

    device_->GetImmediateContext(context_.GetAddressOf());

    DXGI_SWAP_CHAIN_DESC desc{};
    swapChain->GetDesc(&desc);
    hwnd_ = desc.OutputWindow;

    if (!createRenderTarget())
        return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ApplyMacStyle();

    // Load LXGWWenKai as primary font with full Chinese range
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 1;
    io.Fonts->AddFontFromMemoryCompressedTTF(
        LXGWWenKai_Regular_compressed_data, LXGWWenKai_Regular_compressed_size,
        16.0f, &fontCfg, io.Fonts->GetGlyphRangesChineseFull());

    static const ImWchar iconRange[] = { 0xEA01, 0xF2FF, 0 };
    ImFontConfig iconCfg;
    iconCfg.MergeMode = true;
    iconCfg.PixelSnapH = true;
    iconCfg.GlyphMinAdvanceX = 16.0f;

    extern const unsigned int remixicon_compressed_data[];
    extern const unsigned int remixicon_compressed_size;
    io.Fonts->AddFontFromMemoryCompressedTTF(
        remixicon_compressed_data, remixicon_compressed_size,
        16.0f, &iconCfg, iconRange);

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_.Get(), context_.Get());

    originalWndProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wndProc)));

    initialized_ = true;
    LOG_INFO("DX11 Renderer initialized");
    return true;
}

bool DX11Renderer::createRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
        return false;
    if (FAILED(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, rtv_.GetAddressOf())))
        return false;
    return true;
}

void DX11Renderer::destroyRenderTarget() {
    rtv_.Reset();
}

void DX11Renderer::render() {
    if (!initialized_) return;

    if (!rtv_) {
        if (!createRenderTarget()) return;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    GuiManager::Get().render();

    ImGui::EndFrame();
    ImGui::Render();

    context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void DX11Renderer::invalidate() {
    if (!initialized_) return;
    destroyRenderTarget();
}

void DX11Renderer::shutdown() {
    if (!initialized_) return;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (originalWndProc_)
        SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWndProc_));

    destroyRenderTarget();
    initialized_ = false;
}

LRESULT CALLBACK DX11Renderer::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return 1;

    auto& self = DX11Renderer::Get();
    if (GuiManager::Get().isVisible()) {
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN ||
            msg == WM_KEYUP || msg == WM_SYSKEYUP ||
            msg == WM_CHAR || msg == WM_MOUSEMOVE ||
            msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
            msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP ||
            msg == WM_MOUSEWHEEL)
            return 1;
    }

    return CallWindowProcW(self.originalWndProc_, hwnd, msg, wParam, lParam);
}
