#include "pch.h"
#include "features/fps_unlock.h"

void FPSUnlock::onEnable() {
    LOG_INFO("FPS Unlock enabled (target: {})", static_cast<int>(targetFPS));
}

void FPSUnlock::onDisable() {
    LOG_INFO("FPS Unlock disabled, restoring default");
    UApplication::set_targetFrameRate(60);
}

void FPSUnlock::onTick() {
    if (!isEnabled()) return;

    UApplication::set_targetFrameRate(static_cast<int>(targetFPS));
    UQualitySettings::set_vSyncCount(0);
}

void FPSUnlock::drawUI() {
    Widgets::Toggle("Enable##fps", enabled_);

    if (isEnabled()) {
        ImGui::Spacing();
        Widgets::SliderInt("Target FPS", targetFPS, 30, 999);
        ImGui::TextDisabled("999 = uncapped");
    }
}
