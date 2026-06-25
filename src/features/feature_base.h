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
        if (v == isEnabled()) return;
        setEnabledInternal(v);
        v ? onEnable() : onDisable();
    }
    
    bool isEnabled() const { 
        return bPersistEnabled_ ? static_cast<bool>(*enabled_) : enabledRuntime_;
    }
    
    bool& enabledRef() { 
        return bPersistEnabled_ ? enabled_->GetRef() : enabledRuntime_;
    }

protected:
    // 默认构造：使用 ConfigVar 持久化到 config
    // bPersistEnabled=false 时使用纯运行时状态，不保存
    //
    // 示例：
    //   FeatureBase("key")              // 持久化（默认）
    //   FeatureBase("key", true)        // 持久化，初始启用
    //   FeatureBase("key", false, false) // 纯运行时，不持久化
    FeatureBase(const std::string& configKey, bool bDefaultEnabled = false, bool bPersistEnabled = true)
        : bPersistEnabled_(bPersistEnabled),
          enabledRuntime_(bDefaultEnabled),
          enabled_(nullptr) {
        if (bPersistEnabled_) {
            enabled_ = new ConfigVar<bool>(configKey + ".enabled", bDefaultEnabled);
        }
        registry().push_back(this);
    }

    virtual ~FeatureBase() {
        if (bPersistEnabled_ && enabled_) {
            delete enabled_;
        }
    }

private:
    void setEnabledInternal(bool v) {
        if (bPersistEnabled_) {
            **enabled_ = v;
        } else {
            enabledRuntime_ = v;
        }
    }

    bool bPersistEnabled_ = false;          // 是否持久化到 config
    bool enabledRuntime_ = false;           // 仅当 bPersistEnabled_=false 时使用
    ConfigVar<bool>* enabled_ = nullptr;    // 仅当 bPersistEnabled_=true 时使用
};
