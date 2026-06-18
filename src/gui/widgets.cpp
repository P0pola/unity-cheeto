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

    const float height = ImGui::GetFrameHeight() * 0.8f;
    const float width = height * 1.8f;
    const ImVec2 pos = window->DC.CursorPos;

    const ImRect bb(pos, ImVec2(pos.x + width + ImGui::CalcTextSize(label).x + style.ItemInnerSpacing.x, pos.y + height));
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) *v = !*v;

    // Animate
    float t = *v ? 1.0f : 0.0f;
    float animSpeed = 0.08f;
    auto* storage = ImGui::GetStateStorage();
    float anim = storage->GetFloat(id, t);
    if (anim != t) {
        anim = anim + (t - anim) * animSpeed * ImGui::GetIO().DeltaTime * 60.0f;
        if (std::abs(anim - t) < 0.01f) anim = t;
        storage->SetFloat(id, anim);
    }

    // Draw track
    ImU32 trackCol = ImGui::ColorConvertFloat4ToU32(
        ImLerp(ImVec4(0.25f, 0.25f, 0.30f, 1.0f), ImVec4(0.31f, 0.76f, 0.97f, 1.0f), anim));
    float radius = height * 0.5f;
    window->DrawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), trackCol, radius);

    // Draw knob
    float knobRadius = radius * 0.8f;
    float knobX = pos.x + radius + anim * (width - height);
    float knobY = pos.y + radius;
    window->DrawList->AddCircleFilled(ImVec2(knobX, knobY), knobRadius, IM_COL32(255, 255, 255, 240));

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

bool KeyBind(const char* label, int* key) {
    ImGui::PushID(label);
    auto* storage = ImGui::GetStateStorage();
    ImGuiID id = ImGui::GetID("##keybind");
    bool waiting = storage->GetBool(id, false);

    std::string btnText = waiting ? "[...]" : std::format("[{:#x}]", *key);

    if (ImGui::Button(btnText.c_str(), ImVec2(60, 0))) {
        storage->SetBool(id, true);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(label);

    if (waiting) {
        for (int i = 0; i < 256; i++) {
            if (i == VK_INSERT) continue;
            if (GetAsyncKeyState(i) & 1) {
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
