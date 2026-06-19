#pragma once
#include <Windows.h>
#include <functional>

class OpenGLHook {
public:
    static OpenGLHook& Get() { static OpenGLHook inst; return inst; }

    using SwapBuffersCallback = std::function<void(HDC)>;

    bool initialize();
    void shutdown();

    void onSwapBuffers(SwapBuffersCallback cb) { swapCb_ = std::move(cb); }

private:
    OpenGLHook() = default;

    static BOOL WINAPI hkWglSwapBuffers(HDC hdc);

    SwapBuffersCallback swapCb_;
    void* swapTarget_ = nullptr;
    bool initialized_ = false;
};
