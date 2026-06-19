#pragma once
#include <Windows.h>

class OpenGLRenderer {
public:
    static OpenGLRenderer& Get() { static OpenGLRenderer inst; return inst; }

    bool initialize(HDC hdc);
    void render();
    void shutdown();

    bool isReady() const { return initialized_; }

private:
    OpenGLRenderer() = default;

    HWND hwnd_ = nullptr;
    WNDPROC originalWndProc_ = nullptr;
    bool initialized_ = false;

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
