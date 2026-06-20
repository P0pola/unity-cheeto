#include "pch.h"
#include "app.h"
#include "gui/gui_manager.h"
#include "gui/icons.h"
#include "gui/widgets.h"
#include "features/feature_base.h"
#include "features/fps_unlock.h"
#include "features/free_camera.h"
#include "features/world_speed.h"
#include "features/dll_dumper.h"

// Force singleton construction → auto-register into FeatureBase::registry()
static auto& _fps = FPSUnlock::Get();
static auto& _cam = FreeCamera::Get();
static auto& _spd = WorldSpeed::Get();
static auto& _dll = DllDumper::Get();

static ConfigVar<int> g_toggleKey{"menu.toggleKey", VK_INSERT};

namespace App {

void initialize() {
    Translator::GetInstance().initConfig();
    FeatureBase::initAll();

    auto& gui = GuiManager::Get();

    gui.addTab(ICON_RI_PLAYER, "Player", [] {
        FeatureBase::drawCategory("Player");
    });

    gui.addTab(ICON_RI_WORLD, "World", [] {
        FeatureBase::drawCategory("World");
    });

    gui.addTab(ICON_RI_VISUAL, "Visual", [] {
        FeatureBase::drawCategory("Visual");
    });

    gui.addTab(ICON_RI_MISC, "Misc", [] {
        FeatureBase::drawCategory("Misc");
    });

    gui.addTab(ICON_RI_SETTINGS, "Setting", [] {
        ImGui::SeparatorText("Menu");
        static int toggleKeyVal = static_cast<int>(g_toggleKey);
        if (Widgets::KeyBind("Toggle Key", &toggleKeyVal)) {
            g_toggleKey = toggleKeyVal;
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Language");
        Translator::GetInstance().renderLanguageSelector();

        ImGui::Spacing();
        ImGui::SeparatorText("Config");
        if (ImGui::Button("Save"))
            ConfigManager::Instance().Save();
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            ConfigManager::Instance().GetJson().clear();
            ConfigManager::Instance().Save();
        }
    });

    gui.addTab(ICON_RI_BUG, "Debug", [] {
        ImGui::Text("Build: " __DATE__ " " __TIME__);
        ImGui::Separator();

        static int testKey = VK_F1;
        Widgets::KeyBind("Test Hotkey", &testKey);

        if (ImGui::Button("Clear Log"))
            Logger_::clear();
        FeatureBase::drawCategory("Debug");
    });
}

void tick() {
    static bool wasDown = false;
    bool down = (GetAsyncKeyState(static_cast<int>(g_toggleKey)) & 0x8000) != 0;
    if (!down && wasDown)
        GuiManager::Get().toggle();
    wasDown = down;

    HotkeyManager::Get().tick();
    FeatureBase::tickAll();
}

} // namespace App
