#pragma once
#include <string>
#include <vector>
#include <cstring>

class FeatureBase {
public:
    virtual ~FeatureBase() = default;
    virtual void init() {}
    virtual void onEnable() {}
    virtual void onDisable() {}
    virtual void onTick() {}
    virtual void drawUI() {}
    virtual const char* name() const = 0;
    virtual const char* category() const = 0;

    static std::vector<FeatureBase*>& registry() {
        static std::vector<FeatureBase*> r;
        return r;
    }

    static void initAll() {
        LOG_INFO("Loading {} features:", registry().size());
        for (auto* f : registry()) {
            LOG_INFO("  [{}] category={}", f->name(), f->category());
            f->init();
        }
    }

    static void tickAll() {
        for (auto* f : registry()) f->onTick();
    }

    static void drawCategory(const char* cat) {
        for (auto* f : registry())
            if (std::strcmp(f->category(), cat) == 0)
                f->drawUI();
    }

    void setEnabled(bool v) {
        if (v == static_cast<bool>(enabled_)) return;
        enabled_ = v;
        v ? onEnable() : onDisable();
    }
    bool isEnabled() const { return enabled_; }
    bool& enabledRef() { return enabled_.GetRef(); }

protected:
    FeatureBase(const std::string& configKey, bool defaultEnabled = false)
        : enabled_(configKey + ".enabled", defaultEnabled) {
        registry().push_back(this);
    }

    ConfigVar<bool> enabled_;
};
