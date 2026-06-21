#include "pch.h"

#if USE_GFX_API == GFX_API_DX12
#include "renderer/dx12_renderer.h"
#include "renderer/imgui_style.h"
#include "features/feature_base.h"
#include "gui/gui_manager.h"
#include "gui/remixicon.hpp"
#include "gui/LXGWWenKai-Regular.hpp"

#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool DX12Renderer::initialize(IDXGISwapChain* swapChain, ID3D12CommandQueue* cmdQueue) {
    if (initialized_) return true;

    ComPtr<IDXGISwapChain3> sc3;
    if (FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&sc3))))
        return false;
    swapChain_ = sc3;
    cmdQueue_ = cmdQueue;

    if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&device_))))
        return false;

    DXGI_SWAP_CHAIN_DESC desc{};
    swapChain->GetDesc(&desc);
    bufferCount_ = desc.BufferCount;
    hwnd_ = desc.OutputWindow;

    // RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = bufferCount_;
    if (FAILED(device_->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvHeap_))))
        return false;

    // SRV heap for ImGui
    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = 1;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device_->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&srvHeap_))))
        return false;

    // Frame contexts
    frames_.resize(bufferCount_);
    auto rtvStart = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    auto rtvInc = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (UINT i = 0; i < bufferCount_; i++) {
        frames_[i].rtvHandle.ptr = rtvStart.ptr + i * rtvInc;
        if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frames_[i].allocator))))
            return false;
    }

    if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames_[0].allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList_))))
        return false;
    cmdList_->Close();

    if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_))))
        return false;
    fenceEvent_ = CreateEventA(nullptr, FALSE, FALSE, nullptr);

    if (!createRenderTargets())
        return false;

    // ImGui
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

    // Merge RemixIcon font
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
    ImGui_ImplDX12_Init(device_.Get(), bufferCount_, DXGI_FORMAT_R8G8B8A8_UNORM,
        srvHeap_.Get(),
        srvHeap_->GetCPUDescriptorHandleForHeapStart(),
        srvHeap_->GetGPUDescriptorHandleForHeapStart());

    originalWndProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wndProc)));

    initialized_ = true;
    LOG_INFO("DX12 Renderer initialized ({} buffers)", bufferCount_);
    return true;
}

bool DX12Renderer::createRenderTargets() {
    for (UINT i = 0; i < bufferCount_; i++) {
        ComPtr<ID3D12Resource> buf;
        if (FAILED(swapChain_->GetBuffer(i, IID_PPV_ARGS(&buf))))
            return false;
        device_->CreateRenderTargetView(buf.Get(), nullptr, frames_[i].rtvHandle);
        frames_[i].backBuffer = buf;
    }
    return true;
}

void DX12Renderer::destroyRenderTargets() {
    for (auto& frame : frames_)
        frame.backBuffer.Reset();
}

void DX12Renderer::render() {
    if (!initialized_) return;

    // Recreate render targets if invalidated (after resize)
    if (!frames_[0].backBuffer) {
        if (!createRenderTargets()) return;
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    FeatureBase::overlayAll();
    GuiManager::Get().render();

    ImGui::EndFrame();
    ImGui::Render();

    auto frameIdx = swapChain_->GetCurrentBackBufferIndex();
    auto& frame = frames_[frameIdx];

    // Wait until this frame's previous submission is done
    if (fence_->GetCompletedValue() < frame.fenceValue) {
        fence_->SetEventOnCompletion(frame.fenceValue, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    frame.allocator->Reset();
    cmdList_->Reset(frame.allocator.Get(), nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = frame.backBuffer.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList_->ResourceBarrier(1, &barrier);

    cmdList_->OMSetRenderTargets(1, &frame.rtvHandle, FALSE, nullptr);

    ID3D12DescriptorHeap* heaps[] = { srvHeap_.Get() };
    cmdList_->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList_.Get());

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmdList_->ResourceBarrier(1, &barrier);

    cmdList_->Close();

    ID3D12CommandList* cmdLists[] = { cmdList_.Get() };
    cmdQueue_->ExecuteCommandLists(1, cmdLists);

    // Signal for this frame so next time we use it we know it's done
    fenceValue_++;
    frame.fenceValue = fenceValue_;
    cmdQueue_->Signal(fence_.Get(), fenceValue_);
}

void DX12Renderer::waitForGpu() {
    fenceValue_++;
    cmdQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void DX12Renderer::invalidate() {
    if (!initialized_) return;
    waitForGpu();
    destroyRenderTargets();
}

void DX12Renderer::shutdown() {
    if (!initialized_) return;

    waitForGpu();

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (originalWndProc_)
        SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWndProc_));

    destroyRenderTargets();
    if (fenceEvent_) CloseHandle(fenceEvent_);

    initialized_ = false;
}

LRESULT CALLBACK DX12Renderer::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return 1;

    auto& self = DX12Renderer::Get();
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

#endif // USE_GFX_API == GFX_API_DX12
