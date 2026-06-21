#pragma once
#include "features/feature_base.h"

class Noclip : public FeatureBase {
public:
    static Noclip& Get() { static Noclip inst; return inst; }

    bool init() override;
    void onEnable() override;
    void onDisable() override;
    void onTick() override;
    void drawUI() override;

    const char* name() const override { return "Noclip"; }
    const char* category() const override { return "Player"; }

    void tickGameThread(void* hero);

    ConfigVar<float> moveSpeed{"noclip.speed", 15.0f};
    ConfigVar<int> forwardKey{"noclip.key_forward", 'W'};
    ConfigVar<int> backKey{"noclip.key_back", 'S'};
    ConfigVar<int> leftKey{"noclip.key_left", 'A'};
    ConfigVar<int> rightKey{"noclip.key_right", 'D'};
    ConfigVar<int> upKey{"noclip.key_up", VK_SPACE};
    ConfigVar<int> downKey{"noclip.key_down", VK_LCONTROL};

private:
    Noclip() : FeatureBase("noclip") {}

    bool collisionDisabled_ = false;
};
