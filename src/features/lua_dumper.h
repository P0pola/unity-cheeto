#pragma once
#include "features/feature_base.h"

class LuaDumper : public FeatureBase {
public:
    static LuaDumper& Get() { static LuaDumper inst; return inst; }

    bool init() override;
    void onTick() override {}
    void drawUI() override;

    const char* name() const override { return "Lua Dumper"; }
    const char* category() const override { return "Debug"; }

    ConfigVar<std::string> outputDir{"lua_dumper.output_dir", "lua_dump"};
    int dumpCount_ = 0;
    bool hooked_ = false;

private:
    LuaDumper() : FeatureBase("lua_dumper", true) {}
};
