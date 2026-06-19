#include "pch.h"
#include "features/world_speed.h"

void WorldSpeed::init() {
    UTime::set_timeScale_hook([](float value) {
        //auto& ws = WorldSpeed::Get();
        //if (ws.isEnabled())
        //    value = static_cast<float>(ws.speed);
       // LOG_DEBUG("Test hook");
        UTime::set_timeScale_original(value);
        });
}


void WorldSpeed::onEnable() {
    //LOG_INFO("World Speed enabled (scale: {:.2f})", static_cast<float>(speed));
}

void WorldSpeed::onDisable() {
   // LOG_INFO("World Speed disabled, restoring 1.0");
    UTime::set_timeScale(1.0f);
}

void WorldSpeed::onTick() {
    if (!isEnabled()) return;
    UTime::set_timeScale(static_cast<float>(speed));
}

void WorldSpeed::drawUI() {
    if (Widgets::Section("world_speed", TR("World Speed"))) {
        bool en = isEnabled();
        if (Widgets::Toggle(TR("Enable##worldspeed"), &en))
            setEnabled(en);

        if (isEnabled()) {
            ImGui::Spacing();
            Widgets::SliderFloat(TR("Speed##worldspeed"), speed, 0.1f, 10.0f, "%.2f");
        }
        Widgets::SectionEnd();
    }
}
