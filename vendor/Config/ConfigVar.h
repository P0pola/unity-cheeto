#pragma once
#include <utility>
#include <string>
#include <type_traits>

// Forward declaration — ConfigManager.h is included via pch-ue.h
class ConfigManager;

template<typename T>
class ConfigVar {
public:
    // Unbound default (for containers / late init)
    ConfigVar() = default;

    // Self-binding constructor: reads from config or writes default, auto-saves on change
    ConfigVar(const std::string& keyPath, const T& defaultValue) : key_(keyPath)
    {
        auto& cfg = ConfigManager::Instance();
        const auto* existing = cfg.Find(keyPath);
        if (existing) {
            try { value_ = existing->get<T>(); }
            catch (...) {
                value_ = defaultValue;
                cfg.GetOrCreate(keyPath) = defaultValue;
                cfg.Save();
            }
        } else {
            value_ = defaultValue;
            cfg.GetOrCreate(keyPath) = defaultValue;
            cfg.Save();
        }
    }

    ConfigVar(const ConfigVar&) = default;
    ConfigVar(ConfigVar&&) noexcept = default;
    ConfigVar& operator=(const ConfigVar& o) {
        if (this != &o) Set(o.value_);
        return *this;
    }
    ConfigVar& operator=(ConfigVar&&) noexcept = default;

    ConfigVar& operator=(const T& v) { Set(v); return *this; }
    ConfigVar& operator=(T&& v)      { Set(std::move(v)); return *this; }

    operator T() const { return value_; }

    T& GetRef() { return value_; }
    const T& GetRef() const { return value_; }

    // EditProxy — allows direct mutation, triggers save on destruction if changed
    class EditProxy {
    public:
        EditProxy(const EditProxy&) = delete;
        EditProxy& operator=(const EditProxy&) = delete;
        EditProxy(EditProxy&& other) noexcept
            : owner_(other.owner_), backup_(std::move(other.backup_)), active_(other.active_)
        { other.active_ = false; }
        EditProxy& operator=(EditProxy&&) = delete;

        explicit EditProxy(ConfigVar& owner)
            : owner_(&owner), backup_(owner.value_) {}

        operator T& ()             { return owner_->value_; }
        operator const T& () const { return owner_->value_; }
        T* operator&()             { return &owner_->value_; }
        const T* operator&() const { return &owner_->value_; }

        ~EditProxy() {
            if (!active_) return;
            active_ = false;
            if (!(owner_->value_ == backup_))
                owner_->Persist();
        }
    private:
        ConfigVar* owner_ = nullptr;
        T backup_;
        bool active_ = true;
    };

    EditProxy Edit() { return EditProxy(*this); }

private:
    void Set(T v) {
        if (!(value_ == v)) {
            value_ = std::move(v);
            Persist();
        }
    }

    void Persist() {
        if (!key_.empty()) {
            auto& cfg = ConfigManager::Instance();
            cfg.GetOrCreate(key_) = value_;
            cfg.Save();
        }
    }

    T           value_{};
    std::string key_;
};
