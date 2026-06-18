#include "pch.h"
#include "ConfigManager.h"

ConfigManager& ConfigManager::Instance()
{
    static ConfigManager inst;
    return inst;
}

ConfigManager::ConfigManager()
{
    configPath_ = GetExeDir() / "cheetoCfg" / "config.json";
    Load();
}

void ConfigManager::Load()
{
    if (std::filesystem::exists(configPath_)) {
        try {
            std::ifstream in(configPath_);
            in >> root_;
        } catch (...) {
            root_ = nlohmann::json::object();
        }
    } else {
        root_ = nlohmann::json::object();
        Save();
    }
}

void ConfigManager::Save()
{
    std::ofstream out(configPath_);
    out << root_.dump(4);
}

const nlohmann::json* ConfigManager::Find(const std::string& keyPath) const
{
    const nlohmann::json* ptr = &root_;
    size_t start = 0;
    while (true) {
        size_t dot = keyPath.find('.', start);
        std::string part = (dot == std::string::npos)
            ? keyPath.substr(start)
            : keyPath.substr(start, dot - start);
        if (!ptr->is_object() || !ptr->contains(part))
            return nullptr;
        ptr = &(*ptr)[part];
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return ptr;
}

nlohmann::json& ConfigManager::GetOrCreate(const std::string& keyPath)
{
    nlohmann::json* ptr = &root_;
    size_t start = 0;
    while (true) {
        size_t dot = keyPath.find('.', start);
        std::string part = (dot == std::string::npos)
            ? keyPath.substr(start)
            : keyPath.substr(start, dot - start);
        ptr = &(*ptr)[part];
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return *ptr;
}

std::filesystem::path ConfigManager::GetExeDir()
{
    char path[MAX_PATH]{};

    HMODULE hModule = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&ConfigManager::Instance),
                           &hModule)) {
        if (GetModuleFileNameA(hModule, path, MAX_PATH) > 0)
            return std::filesystem::path(path).parent_path();
    }

    if (GetModuleFileNameA(nullptr, path, MAX_PATH) > 0)
        return std::filesystem::path(path).parent_path();

    return std::filesystem::current_path();
}
