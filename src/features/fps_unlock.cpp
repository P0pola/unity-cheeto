#include "pch.h"
#include "features/fps_unlock.h"

//void FPSUnlock::onEnable() {
//    LOG_INFO("FPS Unlock enabled (target: {})", static_cast<int>(targetFPS));
//}
//
//void FPSUnlock::onDisable() {
//    LOG_INFO("FPS Unlock disabled, restoring default");
//    UApplication::set_targetFrameRate(60);
//}

void FPSUnlock::onTick() {
    if (!isEnabled()) return;

    int target = static_cast<int>(targetFPS);
    UApplication::set_targetFrameRate(target);
    UQualitySettings::set_vSyncCount(0);
}

void FPSUnlock::drawUI() {
    if (Widgets::Section("fps_unlock", TR("FPS Unlock"))) {
        bool en = isEnabled();
        if (Widgets::Toggle(TR("Enable##fps"), &en))
            setEnabled(en);

        if (isEnabled()) {
            ImGui::Spacing();
            Widgets::SliderInt(TR("Target FPS"), targetFPS, 30, 300);
        }

        ImGui::Spacing();
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        Widgets::SectionEnd();
    }
}
