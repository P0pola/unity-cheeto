#include "pch.h"

#if USE_GFX_API == GFX_API_OPENGL
#include "renderer/opengl_renderer.h"
#include "renderer/imgui_style.h"
#include "gui/gui_manager.h"
#include "gui/remixicon.hpp"
#include "gui/LXGWWenKai-Regular.hpp"

#include <imgui_impl_win32.h>
#include <imgui_impl_opengl3.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool OpenGLRenderer::initialize(HDC hdc) {
    if (initialized_) return true;

    hwnd_ = WindowFromDC(hdc);
    if (!hwnd_) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ApplyMacStyle();

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

    ImGui_ImplWin32_InitForOpenGL(hwnd_);
    ImGui_ImplOpenGL3_Init();

    originalWndProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wndProc)));

    initialized_ = true;
    LOG_INFO("OpenGL Renderer initialized");
    return true;
}

void OpenGLRenderer::render() {
    if (!initialized_) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    GuiManager::Get().render();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void OpenGLRenderer::shutdown() {
    if (!initialized_) return;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (originalWndProc_)
        SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWndProc_));

    initialized_ = false;
}

LRESULT CALLBACK OpenGLRenderer::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return 1;

    auto& self = OpenGLRenderer::Get();
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

#endif // USE_GFX_API == GFX_API_OPENGL
