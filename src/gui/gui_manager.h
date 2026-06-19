#pragma once
#include <functional>
#include <string>
#include <vector>

class GuiManager {
public:
    static GuiManager& Get() { static GuiManager inst; return inst; }

    struct Tab {
        std::string icon;
        std::string name;
        std::function<void()> draw;
    };

    void addTab(std::string icon, std::string name, std::function<void()> draw) {
        tabs_.push_back({std::move(icon), std::move(name), std::move(draw)});
    }

    void render();
    void toggle() { visible_ = !visible_; }
    bool isVisible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; }

private:
    GuiManager() = default;

    void drawTitleBar();
    void drawSidebar();
    void drawContent();

    std::vector<Tab> tabs_;
    int activeTab_ = 0;
    bool visible_ = true;
    float alpha_ = 0.0f;
};
