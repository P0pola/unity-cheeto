#include "pch.h"
#include "app.h"
#include "gui/gui_manager.h"
#include "gui/icons.h"
#include "gui/widgets.h"
#include "features/feature_base.h"
#include "features/fps_unlock.h"
#include "features/free_camera.h"
#include "features/world_speed.h"
#include "features/unity_dumper.h"
#include "features/esp.h"
#include "features/noclip.h"
//#include "features/lua_dumper.h"
#include "features/lua_executor.h"
#include "features/damage_mult.h"
#include "features/god_mode.h"
#include "features/attack_speed.h"
#include "features/mob_vacuum.h"
#include "features/quest_teleport.h"

// Force singleton construction → auto-register into FeatureBase::registry()
//static auto& _lua = LuaDumper::Get();
static auto& _fps = FPSUnlock::Get();
static auto& _cam = FreeCamera::Get();
static auto& _spd = WorldSpeed::Get();
static auto& _dll = UnityDumper::Get();
static auto& _esp = ESP::Get();
static auto& _noclip = Noclip::Get();
static auto& _luae = LuaExecutor::Get();
static auto& _dmgm = DamageMult::Get();
static auto& _godm = GodMode::Get();
static auto& _atks = AttackSpeed::Get();
static auto& _vacm = MobVacuum::Get();
static auto& _quest = QuestTeleport::Get();

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

        if (ImGui::Button("GM")) {
            LuaExecutor::Get().execute(R"(
do
  Global.IsNeiWangEditShow = true
  if GMController == nil then
    GMController = require("MainGame.UI.Module.GM.GMController"):Init()
  end
  local ResGMCommandPanel = require("MainGame.Generate.Message.ResGMCommandPanel")
  local function BuildGMCommandList(serverList)
    local result = {}
    local exists = {}
    if type(serverList) == "table" then
      for _, v in ipairs(serverList) do
        table.insert(result, v)
        if v ~= nil and v.name ~= nil then exists[v.name] = true end
      end
    end
    local ok, clientList = pcall(function() return GMController:GetClientGMData() end)
    if ok and type(clientList) == "table" then
      for _, v in ipairs(clientList) do
        if v ~= nil and v.name ~= nil and not exists[v.name] then
          table.insert(result, v)
          exists[v.name] = true
        end
      end
    end
    return result
  end
  local function ApplyGMPanel(commandDescList)
    Global.IsNeiWangEditShow = true
    GMController.GMLevel = 2
    local finalList = BuildGMCommandList(commandDescList)
    pcall(function() GMController:InitGMUI() end)
    pcall(function() Dispatcher.Dispatch(EventDefine.GMCommandPanel, finalList) end)
    return finalList
  end
  if not _G.__GMRuntimePatched then
    _G.__GMRuntimePatched = true
    GMController.__OriginResGMCommandPanel = GMController.__OriginResGMCommandPanel or GMController.ResGMCommandPanel
    GMController.ResGMCommandPanel = function(self, msg)
      msg = msg or {}
      ApplyGMPanel(msg.commandDescList)
    end
    SocketManager.F_Register(ResGMCommandPanel, GMController:CreateDelegate(GMController.ResGMCommandPanel))
    function _G.OpenGM()
      Global.IsNeiWangEditShow = true
      GMController.GMLevel = 2
      local list = {}
      local ok, ret = pcall(function() return GMController:GetClientGMData() end)
      if ok and type(ret) == "table" then list = ret end
      ApplyGMPanel(list)
      pcall(function() if UIManager.Get(Window.GMPartUI) == nil then UIManager.Open(Window.GMPartUI) end end)
      pcall(function() if UIManager.Get(Window.GMView) == nil then UIManager.Open(Window.GMView) end end)
    end
    function _G.CloseGM()
      pcall(function() if UIManager.Get(Window.GMView) ~= nil then UIManager.Close(Window.GMView) end end)
      pcall(function() if UIManager.Get(Window.GMPartUI) ~= nil then UIManager.Close(Window.GMPartUI) end end)
    end
    function _G.ToggleGM()
      local hasGMView = false
      pcall(function() hasGMView = UIManager.Get(Window.GMView) ~= nil end)
      if hasGMView then CloseGM() else OpenGM() end
    end
  end
  OpenGM()
end
            )");
        }
        ImGui::SameLine();
        if (ImGui::Button("Dump Assemblies")) {
            LOG_INFO("=== Loaded Assemblies ({}) ===", UnityResolve::assembly.size());
            for (const auto* a : UnityResolve::assembly) {
                LOG_INFO("  [{}] file={}", a->name, a->file);
            }
            LOG_INFO("=== End Assemblies ===");
        }

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
