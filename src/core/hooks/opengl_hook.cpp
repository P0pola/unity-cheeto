#include "pch.h"

#if USE_GFX_API == GFX_API_OPENGL
#include "core/hooks/opengl_hook.h"

#pragma comment(lib, "opengl32.lib")

namespace {
    using WglSwapBuffersFn = BOOL(WINAPI*)(HDC);
    WglSwapBuffersFn oWglSwapBuffers = nullptr;
}

bool OpenGLHook::initialize() {
    if (initialized_) return true;

    LOG_INFO("Initializing OpenGL hooks...");

    HMODULE gl = GetModuleHandleA("opengl32.dll");
    if (!gl) {
        gl = LoadLibraryA("opengl32.dll");
        if (!gl) {
            LOG_ERROR("opengl32.dll not found");
            return false;
        }
    }

    swapTarget_ = reinterpret_cast<void*>(GetProcAddress(gl, "wglSwapBuffers"));
    if (!swapTarget_) {
        LOG_ERROR("wglSwapBuffers not found");
        return false;
    }

    auto& hm = HookManager::getInstance();
    auto detour = reinterpret_cast<WglSwapBuffersFn>(&OpenGLHook::hkWglSwapBuffers);

    if (!HookManager::install(reinterpret_cast<WglSwapBuffersFn>(swapTarget_), detour)) {
        LOG_ERROR("wglSwapBuffers hook failed");
        return false;
    }
    oWglSwapBuffers = hm.getOriginal(detour);

    initialized_ = true;
    LOG_INFO("OpenGL hooks initialized successfully");
    return true;
}

void OpenGLHook::shutdown() {
    if (!initialized_) return;
    HookManager::getInstance().shutdown();
    initialized_ = false;
}

BOOL WINAPI OpenGLHook::hkWglSwapBuffers(HDC hdc) {
    auto& self = OpenGLHook::Get();

    if (self.swapCb_)
        self.swapCb_(hdc);

    return oWglSwapBuffers(hdc);
}

#endif // USE_GFX_API == GFX_API_OPENGL
