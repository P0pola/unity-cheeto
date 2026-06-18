#include "pch.h"
#include "HookManager.h"

HookManager& HookManager::getInstance()
{
    static HookManager instance;
    return instance;
}

HookManager::~HookManager()
{
    shutdown();
}

bool HookManager::initialize()
{
    if (m_initialized) return true;
    // SafetyHook 不需要全局初始化
    m_initialized = true;
    return true;
}

void HookManager::shutdown()
{
    if (!m_initialized) return;

    // Chain hooks must be destroyed LIFO: the inner-most hook (last installed)
    // must be reset first, otherwise resetting an earlier hook restores the
    // target bytes and wipes out the later hook's jmp.
    while (!m_hooks.empty()) m_hooks.pop_back();
    m_detourToOriginal.clear();
    m_initialized = false;
}

bool HookManager::uninstall(void* target)
{
    auto& instance = getInstance();
    if (!instance.m_initialized) return false;

    // Remove every hook on this target, LIFO.
    bool removed = false;
    for (auto it = instance.m_hooks.rbegin(); it != instance.m_hooks.rend();)
    {
        if ((*it)->target != target) { ++it; continue; }

        instance.m_detourToOriginal.erase((*it)->detour);
        // rbegin() + 1 converted to forward iterator for erase
        auto fwd = std::next(it).base();
        instance.m_hooks.erase(fwd);
        it = std::make_reverse_iterator(fwd);
        removed = true;
    }

    return removed;
}

bool HookManager::uninstall(void* target, void* detour)
{
    auto& instance = getInstance();
    if (!instance.m_initialized) return false;

    // Only safe to pop from the top of this target's chain — removing a
    // middle detour would re-expose the lower hook's patched bytes.
    for (auto it = instance.m_hooks.rbegin(); it != instance.m_hooks.rend(); ++it)
    {
        if ((*it)->target != target) continue;
        if ((*it)->detour != detour)
        {
            LOG_ERROR("HookManager::uninstall: detour is not the top of the chain for this target");
            return false;
        }

        instance.m_detourToOriginal.erase((*it)->detour);
        auto fwd = std::next(it).base();
        instance.m_hooks.erase(fwd);
        return true;
    }
    return false;
}

bool HookManager::enableHook(void* target)
{
    if (!initialize() && !m_initialized) return false;

    HookInfo* hook = findHook(target);
    if (!hook) return false;

    if (hook->enabled) return true;

    auto result = hook->hook.enable();
    if (result.has_value())
    {
        hook->enabled = true;
        return true;
    }

    return false;
}

bool HookManager::disableHook(void* target)
{
    if (!initialize() && !m_initialized) return false;

    HookInfo* hook = findHook(target);
    if (!hook) return false;

    if (!hook->enabled) return true;

    auto result = hook->hook.disable();
    if (result.has_value())
    {
        hook->enabled = false;
        return true;
    }

    return false;
}

HookManager::HookInfo* HookManager::findHook(void* target)
{
    // Return the most-recently-installed hook on this target (top of chain).
    for (auto it = m_hooks.rbegin(); it != m_hooks.rend(); ++it)
        if ((*it)->target == target) return it->get();
    return nullptr;
}

void* HookManager::resolveModuleFunction(const std::string& moduleName, intptr_t offset)
{
    HMODULE hModule = GetModuleHandleA(moduleName.c_str());
    if (!hModule)
    {
        hModule = LoadLibraryA(moduleName.c_str());
        if (!hModule)
        {
#ifdef _DEBUG
            std::string debugMsg = "HookManager: Failed to load module " + moduleName + "\n";
            OutputDebugStringA(debugMsg.c_str());
#endif
            return nullptr;
        }
    }

    auto targetFunction = reinterpret_cast<void*>(reinterpret_cast<intptr_t>(hModule) + offset);

#ifdef _DEBUG
    std::string debugMsg = "HookManager: Resolved " + moduleName + " + 0x" +
        std::to_string(offset) + " to " + std::to_string(reinterpret_cast<intptr_t>(targetFunction)) + "\n";
    OutputDebugStringA(debugMsg.c_str());
#endif

    return targetFunction;
}
