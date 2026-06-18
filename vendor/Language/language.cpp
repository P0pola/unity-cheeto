#include "pch.h"
#include "language.h"
#include <fstream>
#include <filesystem>

Translator::Translator() {
    loadFromResource();
}

void Translator::initConfig() {
    if (langConfig_.GetRef() == "简体中文")
        setLanguage(Language::zh_cn);
    else
        setLanguage(Language::en);
}

void Translator::renderLanguageSelector() {
    if (ImGui::BeginCombo("##Language", langConfig_.GetRef().c_str())) {
        if (ImGui::Selectable("English")) {
            setLanguage(Language::en);
            langConfig_ = "English";
        }
        if (ImGui::Selectable("简体中文")) {
            setLanguage(Language::zh_cn);
            langConfig_ = "简体中文";
        }
        ImGui::EndCombo();
    }
}


bool Translator::loadFromResource() {
    auto path = ConfigManager::GetExeDir() / "cheetoCfg" / "lang.json";
    if (std::filesystem::exists(path)) {
        std::ifstream in(path);
        if (in.is_open()) {
            std::string content((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
            if (loadFromString(content)) {
                LOG_INFO("Translator: loaded from {}", path.string());
                return true;
            }
        }
    }

    LOG_WARNING("Translator: lang.json not found in cheetoCfg/");
    return false;
}

bool Translator::reload() {
    return loadFromResource();
}

bool Translator::loadFromString(const std::string& jsonStr) {
    try {
        translations = nlohmann::json::parse(jsonStr);
        return true;
    }
    catch (const nlohmann::json::exception& e) {
        return false;
    }
}

std::string Translator::translate(const std::string& key) const {
    try {
        const auto& translation = translations.at(key);
        const std::string langStr = getLanguageString(currentLanguage);
        
        if (translation.contains(langStr)) {
            return translation[langStr].get<std::string>();
        }
    }
    catch (const nlohmann::json::exception&) {
        // If any error occurs during translation lookup, return the original key
    }
    
    return key;
}

void Translator::setLanguage(Language lang) {
    currentLanguage = lang;

}
Language Translator::getLanguage() const {
    return currentLanguage;
}

std::string Translator::getLanguageString(Language lang) const {
    switch (lang) {
        case Language::en:
            return "en";
        case Language::zh_cn:
            return "zh_cn";
        default:
            return "en";
    }
}