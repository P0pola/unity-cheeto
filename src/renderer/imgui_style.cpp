#include "pch.h"
#include "renderer/imgui_style.h"

void ApplyMacStyle() {
    auto& style = ImGui::GetStyle();
    auto* colors = style.Colors;

    style.WindowRounding = 10.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(10, 5);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 8.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;

    // macOS-like translucent white
    ImVec4 winBg      = ImVec4(0.96f, 0.96f, 0.96f, 0.92f);
    ImVec4 sidebar    = ImVec4(0.90f, 0.90f, 0.92f, 0.95f);
    ImVec4 titlebar   = ImVec4(0.93f, 0.93f, 0.93f, 0.98f);
    ImVec4 accent     = ImVec4(0.20f, 0.47f, 0.96f, 1.00f);
    ImVec4 accentHov  = ImVec4(0.30f, 0.55f, 1.00f, 1.00f);
    ImVec4 surface    = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    ImVec4 surfaceHov = ImVec4(0.85f, 0.85f, 0.88f, 0.80f);
    ImVec4 text       = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    ImVec4 textDim    = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);
    ImVec4 border     = ImVec4(0.78f, 0.78f, 0.80f, 0.60f);

    colors[ImGuiCol_WindowBg]          = winBg;
    colors[ImGuiCol_ChildBg]           = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_PopupBg]           = ImVec4(0.98f, 0.98f, 0.98f, 0.96f);
    colors[ImGuiCol_Border]            = border;

    colors[ImGuiCol_FrameBg]           = surface;
    colors[ImGuiCol_FrameBgHovered]    = surfaceHov;
    colors[ImGuiCol_FrameBgActive]     = ImVec4(0.80f, 0.80f, 0.84f, 0.90f);

    colors[ImGuiCol_TitleBg]           = titlebar;
    colors[ImGuiCol_TitleBgActive]     = titlebar;
    colors[ImGuiCol_TitleBgCollapsed]  = titlebar;

    colors[ImGuiCol_Button]            = surface;
    colors[ImGuiCol_ButtonHovered]     = surfaceHov;
    colors[ImGuiCol_ButtonActive]      = accent;

    colors[ImGuiCol_Header]            = ImVec4(0.20f, 0.47f, 0.96f, 0.15f);
    colors[ImGuiCol_HeaderHovered]     = ImVec4(0.20f, 0.47f, 0.96f, 0.25f);
    colors[ImGuiCol_HeaderActive]      = ImVec4(0.20f, 0.47f, 0.96f, 0.35f);

    colors[ImGuiCol_Tab]               = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TabHovered]        = surfaceHov;
    colors[ImGuiCol_TabSelected]       = accent;
    colors[ImGuiCol_TabDimmed]         = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TabDimmedSelected] = accent;

    colors[ImGuiCol_SliderGrab]        = accent;
    colors[ImGuiCol_SliderGrabActive]  = accentHov;
    colors[ImGuiCol_CheckMark]         = accent;

    colors[ImGuiCol_Separator]         = ImVec4(0.80f, 0.80f, 0.82f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]  = accent;
    colors[ImGuiCol_SeparatorActive]   = accent;

    colors[ImGuiCol_ScrollbarBg]       = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrab]     = ImVec4(0.70f, 0.70f, 0.72f, 0.50f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.55f, 0.55f, 0.58f, 0.70f);
    colors[ImGuiCol_ScrollbarGrabActive]  = accent;

    colors[ImGuiCol_Text]              = text;
    colors[ImGuiCol_TextDisabled]      = textDim;

    colors[ImGuiCol_ResizeGrip]        = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ResizeGripHovered] = accent;
    colors[ImGuiCol_ResizeGripActive]  = accent;
}
