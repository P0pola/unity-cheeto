#include "pch.h"
#include "features/fps_unlock.h"

void FPSUnlock::onEnable() {
    LOG_INFO("FPS Unlock enabled (target: {})", static_cast<int>(targetFPS));
}

void FPSUnlock::onDisable() {
    LOG_INFO("FPS Unlock disabled, restoring default");
    auto* assembly = UnityResolve::Get("UnityEngine.CoreModule.dll");
    if (!assembly) assembly = UnityResolve::Get("UnityEngine.dll");
    if (!assembly) return;

    auto* appClass = assembly->Get("Application");
    if (!appClass) return;

    if (auto* setter = appClass->Get<UnityResolve::Method>("set_targetFrameRate", {"System.Int32"}))
        setter->Invoke<void>(60);
}

void FPSUnlock::onTick() {
    if (!isEnabled()) return;

    auto* assembly = UnityResolve::Get("UnityEngine.CoreModule.dll");
    if (!assembly) assembly = UnityResolve::Get("UnityEngine.dll");
    if (!assembly) return;

    auto* appClass = assembly->Get("Application");
    if (appClass) {
        if (auto* setter = appClass->Get<UnityResolve::Method>("set_targetFrameRate", {"System.Int32"}))
            setter->Invoke<void>(static_cast<int>(targetFPS));
    }

    auto* qsClass = assembly->Get("QualitySettings");
    if (qsClass) {
        if (auto* setter = qsClass->Get<UnityResolve::Method>("set_vSyncCount", {"System.Int32"}))
            setter->Invoke<void>(0);
    }
}

void FPSUnlock::drawUI() {
    Widgets::Toggle("Enable##fps", enabled_);

    if (isEnabled()) {
        ImGui::Spacing();
        Widgets::SliderInt("Target FPS", targetFPS, 30, 999);
        ImGui::TextDisabled("999 = uncapped");
    }
}
