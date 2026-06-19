#pragma once
#include "features/feature_base.h"

class FPSUnlock : public FeatureBase {
public:
    static FPSUnlock& Get() { static FPSUnlock inst; return inst; }

    //void onEnable() override;
    //void onDisable() override;
    void onTick() override;
    void drawUI() override;

    const char* name() const override { return "FPS Unlock"; }

    ConfigVar<int> targetFPS{"fps_unlock.target", 999};

private:
    FPSUnlock() : FeatureBase("fps_unlock") {}
};
