#pragma once
#include "features/feature_base.h"

class FreeCamera : public FeatureBase {
public:
    static FreeCamera& Get() { static FreeCamera inst; return inst; }

    void onEnable() override;
    void onDisable() override;
    void onTick() override;
    void drawUI() override;

    const char* name() const override { return "Free Camera"; }

    ConfigVar<float> moveSpeed{"freecam.speed", 10.0f};
    ConfigVar<float> sensitivity{"freecam.sensitivity", 2.0f};
    ConfigVar<int> forwardKey{"freecam.key_forward", 'W'};
    ConfigVar<int> backKey{"freecam.key_back", 'S'};
    ConfigVar<int> leftKey{"freecam.key_left", 'A'};
    ConfigVar<int> rightKey{"freecam.key_right", 'D'};
    ConfigVar<int> upKey{"freecam.key_up", VK_SPACE};
    ConfigVar<int> downKey{"freecam.key_down", VK_LCONTROL};

private:
    FreeCamera() : FeatureBase("freecam") {}

    void* cameraObj_ = nullptr;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    POINT lastMouse_{};
};
