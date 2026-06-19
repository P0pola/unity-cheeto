#pragma once
#include "features/feature_base.h"

class WorldSpeed : public FeatureBase {
public:
    static WorldSpeed& Get() { static WorldSpeed inst; return inst; }
    void init() override;
    void onEnable() override;
    void onDisable() override;
    void onTick() override;
    void drawUI() override;

    const char* name() const override { return "World Speed"; }
    const char* category() const override { return "World"; }

    ConfigVar<float> speed{"world_speed.speed", 1.0f};

private:
    WorldSpeed() : FeatureBase("world_speed") {}
};
