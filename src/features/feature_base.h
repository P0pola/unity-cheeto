#pragma once
#include <string>
#include <vector>
#include <cstring>
#include <chrono>

class FeatureBase {
public:
    virtual ~FeatureBase() = default;
    virtual bool init() { return true; }
    virtual void onEnable() {}
    virtual void onDisable() {}
    virtual void onTick() {}
    virtual void drawOverlay() {}
    virtual void drawUI() {}
    virtual const char* name() const = 0;
    virtual const char* category() const = 0;

    static std::vector<FeatureBase*>& registry() {
        static std::vector<FeatureBase*> r;
        return r;
    }

    static std::vector<FeatureBase*>& pendingInit() {
        static std::vector<FeatureBase*> p;
        return p;
    }

    static void initAll() {
        LOG_INFO("Loading {} features:", registry().size());
        for (auto* f : registry()) {
            LOG_INFO("  [{}] category={}", f->name(), f->category());
            if (!f->init()) {
                pendingInit().push_back(f);
            }
        }
        if (!pendingInit().empty()) {
            LOG_INFO("{} features pending (waiting for assemblies)", pendingInit().size());
        }
    }

    static void retryPending() {
        if (pendingInit().empty()) return;
        static auto lastRetry = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastRetry < std::chrono::seconds(3)) return;
        lastRetry = now;

        auto& pending = pendingInit();
        for (auto it = pending.begin(); it != pending.end();) {
            if ((*it)->init()) {
                LOG_INFO("[{}] late-init OK", (*it)->name());
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
    }

    static void tickAll() {
        retryPending();
        for (auto* f : registry()) f->onTick();
    }

    static void overlayAll() {
        for (auto* f : registry()) f->drawOverlay();
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
