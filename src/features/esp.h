#pragma once
#include "features/feature_base.h"
#include <mutex>
#include <vector>
#include <string>

class ESP : public FeatureBase {
public:
    static ESP& Get() { static ESP inst; return inst; }

    bool init() override;
    void drawOverlay() override;
    void drawUI() override;

    const char* name() const override { return "ESP"; }
    const char* category() const override { return "Visual"; }

    ConfigVar<bool> showMonsters{"esp.monsters", true};
    ConfigVar<bool> showPlayers{"esp.players", true};
    ConfigVar<bool> showNpcs{"esp.npcs", false};
    ConfigVar<bool> showLine{"esp.line", true};
    ConfigVar<bool> showDistance{"esp.distance", true};
    ConfigVar<bool> showName{"esp.name", true};
    ConfigVar<float> maxDistance{"esp.max_distance", 100.0f};

    float monsterColor[4] = {1.0f, 0.27f, 0.27f, 1.0f};
    float playerColor[4] = {0.27f, 1.0f, 0.27f, 1.0f};
    float npcColor[4] = {1.0f, 1.0f, 0.27f, 1.0f};

    static inline void* cachedObjectManager = nullptr;

    struct EntityCache {
        Vector3 pos;
        float screenX, screenY;
        float dist;
        int type;
        bool onScreen;
        std::string name;
    };

    void updateCache();

private:
    ESP() : FeatureBase("esp") {}

    void dumpEntityInfo();

    std::mutex cacheMutex_;
    std::vector<EntityCache> entities_;
    Vector3 heroPos_{};
    int screenW_ = 0;
    int screenH_ = 0;
};
