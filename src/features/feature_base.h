#pragma once
#include <string>

class FeatureBase {
public:
    virtual ~FeatureBase() = default;
    virtual void onEnable() {}
    virtual void onDisable() {}
    virtual void onTick() {}
    virtual void drawUI() {}
    virtual const char* name() const = 0;

    void setEnabled(bool v) {
        if (v == static_cast<bool>(enabled_)) return;
        enabled_ = v;
        v ? onEnable() : onDisable();
    }
    bool isEnabled() const { return enabled_; }
    bool& enabledRef() { return enabled_.GetRef(); }

protected:
    FeatureBase(const std::string& configKey, bool defaultEnabled = false)
        : enabled_(configKey + ".enabled", defaultEnabled) {}

    ConfigVar<bool> enabled_;
};
