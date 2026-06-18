#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include "../safetyhook/safetyhook.hpp"
#include "../logger/logger.h"

#ifdef _DEBUG
#define CALL_ORIGINAL(handler, ...) \
HookManager::getInstance().callOriginal(handler, __func__, __VA_ARGS__)
#else
#define CALL_ORIGIN(handler, ...) \
HookManager::getInstance().callOriginal(handler, __VA_ARGS__)
#endif

class HookManager
{
public:
    struct HookInfo
    {
        void* target;
        void* detour;
        void* original;
        bool enabled;
        std::string moduleName;
        intptr_t offset;
        safetyhook::InlineHook hook;

        HookInfo(void* t, void* d, void* o, safetyhook::InlineHook h, std::string module = "", intptr_t off = 0)
            : target(t)
            , detour(d)
            , original(o)
            , enabled(false)
            , moduleName(std::move(module))
            , offset(off)
            , hook(std::move(h))
        {
        }
    };

    static HookManager& getInstance();

    void shutdown();

    template <typename T>
    static bool install(T target, T detour);

    template <typename T>
    static bool install(const std::string& moduleName, intptr_t offset, T detour);

    static bool uninstall(void* target);
    static bool uninstall(void* target, void* detour);

    bool enableHook(void* target);
    bool disableHook(void* target);

    template <typename T>
    T getOriginal(T handler) const;

#ifdef _DEBUG
    template <typename RType, typename... Params>
    RType callOriginal(RType (*handler)(Params...), const char* callerName, Params... params);
#else
    template <typename RType, typename... Params>
    RType callOriginal(RType (*handler)(Params...), Params... params);
#endif

    _NODISCARD bool isInitialized() const { return m_initialized; }
    _NODISCARD const std::vector<std::unique_ptr<HookInfo>>& getHooks() const { return m_hooks; }

private:
    HookManager() = default;
    ~HookManager();

    HookManager(const HookManager&) = delete;
    HookManager& operator=(const HookManager&) = delete;

    std::vector<std::unique_ptr<HookInfo>> m_hooks;
    std::unordered_map<void*, void*> m_detourToOriginal;
    bool m_initialized = false;

    bool initialize();
    HookInfo* findHook(void* target);
    void* resolveModuleFunction(const std::string& moduleName, intptr_t offset);
};

// ---- Template implementations ----

template <typename T>
bool HookManager::install(T target, T detour)
{
    auto& instance = getInstance();
    if (!instance.initialize() && !instance.m_initialized) return false;

    void* pTarget = reinterpret_cast<void*>(target);
    void* pDetour = reinterpret_cast<void*>(detour);

    // Chain-hook: same target may be hooked by multiple detours. Each new
    // create_inline reads the target's current bytes (already a jmp into the
    // previous detour), so the new trampoline naturally calls the previous
    // detour as its "original", forming a LIFO chain.
    auto hook = safetyhook::create_inline(pTarget, pDetour);
    if (!hook)
    {
        LOG_ERROR("Failed to create hook");
        return false;
    }

    void* originalPtr = hook.original<void*>();
    auto hookInfo = std::make_unique<HookInfo>(pTarget, pDetour, originalPtr, std::move(hook));
    hookInfo->enabled = true;
    instance.m_hooks.push_back(std::move(hookInfo));
    instance.m_detourToOriginal[pDetour] = originalPtr;

    return true;
}

template <typename T>
bool HookManager::install(const std::string& moduleName, intptr_t offset, T detour)
{
    auto& instance = getInstance();
    if (!instance.initialize() && !instance.m_initialized) return false;

    void* target = instance.resolveModuleFunction(moduleName, offset);
    if (!target) return false;

    void* pDetour = reinterpret_cast<void*>(detour);

    auto hook = safetyhook::create_inline(target, pDetour);
    if (!hook)
    {
        LOG_ERROR("Failed to create hook");
        return false;
    }

    void* originalPtr = hook.original<void*>();
    auto hookInfo = std::make_unique<HookInfo>(target, pDetour, originalPtr, std::move(hook), moduleName, offset);
    hookInfo->enabled = true;
    instance.m_hooks.push_back(std::move(hookInfo));
    instance.m_detourToOriginal[pDetour] = originalPtr;

    return true;
}

template <typename T>
T HookManager::getOriginal(T handler) const
{
    auto it = m_detourToOriginal.find(reinterpret_cast<void*>(handler));
    if (it != m_detourToOriginal.end()) return reinterpret_cast<T>(it->second);

#ifdef _DEBUG
    OutputDebugStringA("HookManager: Original function not found for handler\n");
#endif

    return nullptr;
}

#ifdef _DEBUG
template <typename RType, typename... Params>
RType HookManager::callOriginal(RType (*handler)(Params...), const char* callerName, Params... params)
{
    auto original = getOriginal(handler);
    if (original != nullptr)
        return original(params...);

    if (callerName)
    {
        std::string debugMsg = "HookManager: Original function not found for handler in " + std::string(callerName) +
            "\n";
        OutputDebugStringA(debugMsg.c_str());
    }

    return RType{};
}
#else
template <typename RType, typename... Params>
RType HookManager::callOriginal(RType (*handler)(Params...), Params... params)
{
    auto original = getOriginal(handler);
    if (original != nullptr) return original(params...);

    return RType{};
}
#endif
