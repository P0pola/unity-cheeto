#include "pch.h"
#include "gui/widgets.h"
#include <imgui_internal.h>

namespace Widgets {

bool Toggle(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *ImGui::GetCurrentContext();
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImGuiID animId = id + 1;

    const float height = ImGui::GetFrameHeight() * 0.8f;
    const float width = height * 1.8f;
    const ImVec2 pos = window->DC.CursorPos;

    const ImRect bb(pos, ImVec2(pos.x + width + ImGui::CalcTextSize(label).x + style.ItemInnerSpacing.x, pos.y + height));
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) *v = !*v;

    // Smooth knob slide animation
    float target = *v ? 1.0f : 0.0f;
    auto* storage = ImGui::GetStateStorage();
    float anim = storage->GetFloat(animId, -1.0f);
    if (anim < 0.0f) {
        anim = target;
        storage->SetFloat(animId, anim);
    }
    if (pressed) {
        // Don't snap — keep current anim, let it interpolate toward new target
    }
    if (anim != target) {
        float step = ImGui::GetIO().DeltaTime / 0.2f;
        if (target > anim)
            anim = std::min(anim + step, target);
        else
            anim = std::max(anim - step, target);
        storage->SetFloat(animId, anim);
    }

    // Draw track with gradient feel
    ImVec4 offCol(0.25f, 0.25f, 0.30f, 1.0f);
    ImVec4 onCol(0.20f, 0.47f, 0.96f, 1.0f);
    ImU32 trackCol = ImGui::ColorConvertFloat4ToU32(ImLerp(offCol, onCol, anim));
    float radius = height * 0.5f;
    window->DrawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), trackCol, radius);

    // Draw knob with shadow
    float knobRadius = radius * 0.82f;
    float knobX = pos.x + radius + anim * (width - height);
    float knobY = pos.y + radius;
    window->DrawList->AddCircleFilled(ImVec2(knobX, knobY + 1.0f), knobRadius + 0.5f, IM_COL32(0, 0, 0, 30));
    window->DrawList->AddCircleFilled(ImVec2(knobX, knobY), knobRadius, IM_COL32(255, 255, 255, 245));

    // Label
    ImGui::RenderText(ImVec2(pos.x + width + style.ItemInnerSpacing.x, pos.y + (height - ImGui::GetTextLineHeight()) * 0.5f), label);

    return pressed;
}

bool SliderFloat(const char* label, float* v, float min, float max, const char* fmt) {
    return ImGui::SliderFloat(label, v, min, max, fmt);
}

bool SliderInt(const char* label, int* v, int min, int max) {
    return ImGui::SliderInt(label, v, min, max);
}

static const char* GetKeyName(int vk) {
    switch (vk) {
    case 0: return "None";
    case VK_LBUTTON: return "Mouse1";
    case VK_RBUTTON: return "Mouse2";
    case VK_MBUTTON: return "Mouse3";
    case VK_XBUTTON1: return "Mouse4";
    case VK_XBUTTON2: return "Mouse5";
    case VK_BACK: return "Backspace";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU: return "Alt";
    case VK_PAUSE: return "Pause";
    case VK_CAPITAL: return "CapsLock";
    case VK_ESCAPE: return "Esc";
    case VK_SPACE: return "Space";
    case VK_PRIOR: return "PageUp";
    case VK_NEXT: return "PageDown";
    case VK_END: return "End";
    case VK_HOME: return "Home";
    case VK_LEFT: return "Left";
    case VK_UP: return "Up";
    case VK_RIGHT: return "Right";
    case VK_DOWN: return "Down";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_LSHIFT: return "LShift";
    case VK_RSHIFT: return "RShift";
    case VK_LCONTROL: return "LCtrl";
    case VK_RCONTROL: return "RCtrl";
    case VK_LMENU: return "LAlt";
    case VK_RMENU: return "RAlt";
    case VK_OEM_3: return "`";
    case VK_OEM_MINUS: return "-";
    case VK_OEM_PLUS: return "=";
    case VK_OEM_4: return "[";
    case VK_OEM_6: return "]";
    case VK_OEM_5: return "\\";
    case VK_OEM_1: return ";";
    case VK_OEM_7: return "'";
    case VK_OEM_COMMA: return ",";
    case VK_OEM_PERIOD: return ".";
    case VK_OEM_2: return "/";
    default:
        if (vk >= '0' && vk <= '9') { static char num[2]; num[0] = (char)vk; num[1] = 0; return num; }
        if (vk >= 'A' && vk <= 'Z') { static char letter[2]; letter[0] = (char)vk; letter[1] = 0; return letter; }
        if (vk >= VK_F1 && vk <= VK_F12) { static char fn[4]; snprintf(fn, sizeof(fn), "F%d", vk - VK_F1 + 1); return fn; }
        if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) { static char np[6]; snprintf(np, sizeof(np), "Num%d", vk - VK_NUMPAD0); return np; }
        static char hex[8]; snprintf(hex, sizeof(hex), "0x%02X", vk); return hex;
    }
}

bool KeyBind(const char* label, int* key) {
    ImGui::PushID(label);
    auto* storage = ImGui::GetStateStorage();
    ImGuiID id = ImGui::GetID("##keybind");
    bool waiting = storage->GetBool(id, false);

    // Animate waiting state (0→1 when entering wait, 1→0 when leaving)
    ImGuiID animId = ImGui::GetID("##kb_anim");
    float animTarget = waiting ? 1.0f : 0.0f;
    float anim = storage->GetFloat(animId, 0.0f);
    if (anim != animTarget) {
        float speed = 10.0f * ImGui::GetIO().DeltaTime;
        anim += (animTarget - anim) * std::min(speed, 1.0f);
        if (std::abs(anim - animTarget) < 0.005f) anim = animTarget;
        storage->SetFloat(animId, anim);
    }

    // Pulse effect when waiting
    float pulse = 0.0f;
    if (anim > 0.01f) {
        pulse = (sinf((float)ImGui::GetTime() * 4.0f) * 0.5f + 0.5f) * anim;
    }

    // Button colors based on state
    ImVec4 normalCol(0.88f, 0.88f, 0.90f, 1.0f);
    ImVec4 waitCol(0.31f, 0.76f, 0.97f, 1.0f);
    ImVec4 btnCol = ImLerp(normalCol, waitCol, anim);

    // Pulse brightens the button
    btnCol.x += pulse * 0.1f;
    btnCol.y += pulse * 0.1f;
    btnCol.z += pulse * 0.05f;

    ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(btnCol.x + 0.05f, btnCol.y + 0.05f, btnCol.z + 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(btnCol.x - 0.05f, btnCol.y - 0.05f, btnCol.z - 0.05f, 1.0f));

    // Text color: dark on light bg, white on colored bg
    ImVec4 textCol = ImLerp(ImVec4(0.1f, 0.1f, 0.1f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), anim);
    ImGui::PushStyleColor(ImGuiCol_Text, textCol);

    std::string btnText = waiting ? "..." : GetKeyName(*key);

    // Scale animation on press
    float scale = 1.0f - anim * 0.02f * pulse;
    float btnW = 70.0f * scale;

    if (ImGui::Button(btnText.c_str(), ImVec2(btnW, 0))) {
        storage->SetBool(id, true);
    }

    ImGui::PopStyleColor(4);

    ImGui::SameLine();
    ImGui::TextUnformatted(label);

    if (waiting) {
        for (int i = 0; i < 256; i++) {
            if (i == VK_LBUTTON) continue;
            if (GetAsyncKeyState(i) & 0x8000) {
                if (i == VK_ESCAPE) {
                    storage->SetBool(id, false);
                } else {
                    *key = i;
                    storage->SetBool(id, false);
                }
                break;
            }
        }
    }

    ImGui::PopID();
    return !waiting;
}

bool CategoryHeader(const char* label, bool* open) {
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.20f, 1.0f));
    bool result = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopStyleColor();
    if (open) *open = result;
    return result;
}

bool Section(const char* id, const char* displayText, bool defaultOpen) {
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.03f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.0f, 0.0f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoAutoOpenOnLog;
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    // Build "displayText##id" so ImGui uses id for hashing, displayText for display
    char buf[256];
    snprintf(buf, sizeof(buf), "%s##%s", displayText, id);

    bool isOpen = ImGui::TreeNodeEx(buf, flags);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    return isOpen;
}

void SectionEnd() {
    ImGui::TreePop();
}

void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(300.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool Toggle(const char* label, ConfigVar<bool>& cv) {
    bool v = cv;
    if (Toggle(label, &v)) { cv = v; return true; }
    return false;
}

bool SliderFloat(const char* label, ConfigVar<float>& cv, float min, float max, const char* fmt) {
    auto proxy = cv.Edit();
    float v = static_cast<float>(proxy);
    if (ImGui::SliderFloat(label, &v, min, max, fmt)) { static_cast<float&>(proxy) = v; return true; }
    return false;
}

bool SliderInt(const char* label, ConfigVar<int>& cv, int min, int max) {
    auto proxy = cv.Edit();
    int v = static_cast<int>(proxy);
    if (ImGui::SliderInt(label, &v, min, max)) { static_cast<int&>(proxy) = v; return true; }
    return false;
}

bool KeyBind(const char* label, ConfigVar<int>& cv) {
    int k = cv;
    if (KeyBind(label, &k)) { cv = k; return true; }
    return false;
}

} // namespace Widgets
