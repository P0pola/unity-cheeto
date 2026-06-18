#pragma once
#include <string>
#include <unordered_map>


#define TR(key) Translator::GetInstance().translate(XS(key)).c_str()
#define TR_(key) Translator::GetInstance().translate(key).c_str()


enum class Language : int32_t {
    en,
    zh_cn
};

class Translator {
public:
    static Translator& GetInstance() {
        static Translator instance;
        return instance;
    };
    virtual ~Translator() = default;

    Translator(const Translator&) = delete;
    Translator& operator=(const Translator&) = delete;

    // Applies saved language setting
    void initConfig();

    // Renders the ImGui language selector combo
    void renderLanguageSelector();

    bool loadFromString(const std::string& jsonStr);
    std::string translate(const std::string& key) const;
    void setLanguage(Language lang);
    Language getLanguage() const;
    bool reload();

private:
    Translator();

    bool loadFromResource();
    Language currentLanguage = Language::en;
    nlohmann::json translations;
    ConfigVar<std::string> langConfig_{"Language.type", std::string("English")};

    std::string getLanguageString(Language lang) const;
};