#include "pch.h"
#include "gui/gui_manager.h"
#include <imgui_internal.h>

static constexpr float TITLEBAR_H = 32.0f;
static constexpr float SIDEBAR_W = 56.0f;

void GuiManager::render() {
    if (!visible_ && alpha_ <= 0.0f) return;

    float target = visible_ ? 1.0f : 0.0f;
    alpha_ += (target - alpha_) * 0.14f;
    if (alpha_ < 0.01f) { alpha_ = 0.0f; return; }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::SetNextWindowSize(ImVec2(560, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("##cheeto_main", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);

    drawTitleBar();
    drawSidebar();
    drawContent();

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void GuiManager::drawTitleBar() {
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Title bar background
    ImVec2 tbMin = winPos;
    ImVec2 tbMax = ImVec2(winPos.x + winSize.x, winPos.y + TITLEBAR_H);
    dl->AddRectFilled(tbMin, tbMax, IM_COL32(235, 235, 237, 250),
        ImGui::GetStyle().WindowRounding, ImDrawFlags_RoundCornersTop);

    // Bottom border
    dl->AddLine(ImVec2(tbMin.x, tbMax.y), ImVec2(tbMax.x, tbMax.y),
        IM_COL32(200, 200, 202, 150));

    // Traffic light buttons (close, minimize, maximize)
    float btnRadius = 6.0f;
    float btnY = winPos.y + TITLEBAR_H * 0.5f;
    float btnStartX = winPos.x + 16.0f;
    float btnSpacing = 20.0f;

    struct TrafficBtn { ImU32 color; ImU32 hoverColor; };
    TrafficBtn buttons[3] = {
        { IM_COL32(255, 95, 87, 255),  IM_COL32(255, 70, 60, 255) },   // close (red)
        { IM_COL32(255, 189, 46, 255), IM_COL32(230, 170, 30, 255) },  // minimize (yellow)
        { IM_COL32(40, 200, 64, 255),  IM_COL32(30, 180, 50, 255) },   // maximize (green)
    };

    const char* btnIds[3] = { "##tb_close", "##tb_min", "##tb_max" };

    for (int i = 0; i < 3; i++) {
        ImVec2 center(btnStartX + i * btnSpacing, btnY);
        ImRect btnRect(ImVec2(center.x - btnRadius, center.y - btnRadius),
                      ImVec2(center.x + btnRadius, center.y + btnRadius));
        ImGuiID id = ImGui::GetID(btnIds[i]);

        bool hovered = false;
        if (ImGui::ItemAdd(btnRect, id)) {
            bool held;
            if (ImGui::ButtonBehavior(btnRect, id, &hovered, &held)) {
                if (i == 0) visible_ = false;
            }
        }

        ImU32 col = hovered ? buttons[i].hoverColor : buttons[i].color;
        dl->AddCircleFilled(center, btnRadius, col);
        dl->AddCircle(center, btnRadius, IM_COL32(0, 0, 0, 30));
    }

    // Logo / title text
    float titleX = winPos.x + SIDEBAR_W + 12.0f;
    dl->AddText(ImVec2(titleX, winPos.y + (TITLEBAR_H - ImGui::GetTextLineHeight()) * 0.5f),
        IM_COL32(80, 80, 85, 255), "UnityCheeto");
}

void GuiManager::drawSidebar() {
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Sidebar background
    ImVec2 sbMin(winPos.x, winPos.y + TITLEBAR_H);
    ImVec2 sbMax(winPos.x + SIDEBAR_W, winPos.y + winSize.y);
    dl->AddRectFilled(sbMin, sbMax, IM_COL32(230, 230, 232, 242),
        ImGui::GetStyle().WindowRounding, ImDrawFlags_RoundCornersBottomLeft);

    // Right border
    dl->AddLine(ImVec2(sbMax.x, sbMin.y), ImVec2(sbMax.x, sbMax.y),
        IM_COL32(200, 200, 202, 150));

    // Tab buttons
    float btnSize = 40.0f;
    float startY = sbMin.y + 12.0f;
    float padX = (SIDEBAR_W - btnSize) * 0.5f;
    float spacing = btnSize + 6.0f;

    ImGui::SetCursorScreenPos(ImVec2(sbMin.x + padX, startY));

    // Animate active tab indicator position
    auto* storage = ImGui::GetStateStorage();
    ImGuiID sliderId = ImGui::GetID("##tab_slider");
    float targetY = startY + activeTab_ * spacing;
    float sliderY = storage->GetFloat(sliderId, targetY);
    if (sliderY != targetY) {
        float speed = 12.0f * ImGui::GetIO().DeltaTime;
        sliderY += (targetY - sliderY) * std::min(speed, 1.0f);
        if (std::abs(sliderY - targetY) < 0.5f) sliderY = targetY;
        storage->SetFloat(sliderId, sliderY);
    }

    // Draw sliding active background
    ImVec2 activeMin(sbMin.x + padX, sliderY);
    ImVec2 activeMax(sbMin.x + padX + btnSize, sliderY + btnSize);
    dl->AddRectFilled(activeMin, activeMax, IM_COL32(51, 120, 245, 200), 8.0f);

    for (int i = 0; i < static_cast<int>(tabs_.size()); i++) {
        ImVec2 btnPos(sbMin.x + padX, startY + i * spacing);
        ImRect btnRect(btnPos, ImVec2(btnPos.x + btnSize, btnPos.y + btnSize));

        ImGuiID id = ImGui::GetID((std::string("##tab") + std::to_string(i)).c_str());
        ImGui::ItemAdd(btnRect, id);

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(btnRect, id, &hovered, &held);
        if (pressed) activeTab_ = i;

        bool active = (i == activeTab_);

        // Hover highlight (only for non-active tabs)
        if (!active && hovered) {
            ImGuiID hoverId = ImGui::GetID((std::string("##tabh") + std::to_string(i)).c_str());
            float hoverAnim = storage->GetFloat(hoverId, 0.0f);
            hoverAnim += (1.0f - hoverAnim) * std::min(10.0f * ImGui::GetIO().DeltaTime, 1.0f);
            storage->SetFloat(hoverId, hoverAnim);
            dl->AddRectFilled(btnRect.Min, btnRect.Max,
                IM_COL32(0, 0, 0, (int)(20 * hoverAnim)), 8.0f);
        } else if (!active) {
            ImGuiID hoverId = ImGui::GetID((std::string("##tabh") + std::to_string(i)).c_str());
            float hoverAnim = storage->GetFloat(hoverId, 0.0f);
            if (hoverAnim > 0.01f) {
                hoverAnim *= std::max(1.0f - 10.0f * ImGui::GetIO().DeltaTime, 0.0f);
                storage->SetFloat(hoverId, hoverAnim);
                dl->AddRectFilled(btnRect.Min, btnRect.Max,
                    IM_COL32(0, 0, 0, (int)(20 * hoverAnim)), 8.0f);
            }
        }

        // Icon text centered — color lerps based on distance from slider
        float dist = std::abs(btnPos.y - sliderY) / spacing;
        float iconAlpha = std::max(1.0f - dist, 0.0f);
        ImU32 textCol = active
            ? IM_COL32(255, 255, 255, (int)(255 * iconAlpha + 220 * (1.0f - iconAlpha)))
            : IM_COL32(80, 80, 90, 220);
        if (active) textCol = IM_COL32(255, 255, 255, 255);

        ImVec2 textSize = ImGui::CalcTextSize(tabs_[i].icon.c_str());
        ImVec2 textPos(
            btnPos.x + (btnSize - textSize.x) * 0.5f,
            btnPos.y + (btnSize - textSize.y) * 0.5f
        );
        dl->AddText(textPos, textCol, tabs_[i].icon.c_str());
    }
}

void GuiManager::drawContent() {
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();

    float contentX = winPos.x + SIDEBAR_W + 1.0f;
    float contentY = winPos.y + TITLEBAR_H + 1.0f;
    float contentW = winSize.x - SIDEBAR_W - 1.0f;
    float contentH = winSize.y - TITLEBAR_H - 1.0f;

    ImGui::SetCursorScreenPos(ImVec2(contentX + 16.0f, contentY + 12.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 12));
    ImGui::BeginChild("##content", ImVec2(contentW - 32.0f, contentH - 24.0f), false);

    if (activeTab_ >= 0 && activeTab_ < static_cast<int>(tabs_.size())) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
        tabs_[activeTab_].draw();
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}
