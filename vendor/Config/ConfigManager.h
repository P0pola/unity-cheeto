#pragma once
#include <string>
#include <filesystem>

class ConfigManager
{
public:
    static ConfigManager& Instance();


    const nlohmann::json* Find(const std::string& keyPath) const;
    nlohmann::json&       GetOrCreate(const std::string& keyPath);

    void Save();

    nlohmann::json& GetJson() { return root_; }

private:
    ConfigManager();
    void Load();

    nlohmann::json        root_;
    std::filesystem::path configPath_;

public:
    static std::filesystem::path GetExeDir();
};
