#include "pch.h"
#include "features/esp.h"

using IList = UnityResolve::UnityType::List<void*>;

static void hkObjectManagerUpdate(void* self) {
    ESP::cachedObjectManager = self;
    if (ESP::Get().isEnabled()) {
        __try {
            ESP::Get().updateCache();
        } __except(EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    ObjectManager::Update_original(self);
}

void ESP::init() {
    LOG_INFO("[ESP] init start");

    auto* klass = ObjectManager::uclass();
    if (!klass) {
        LOG_ERROR("[ESP] ObjectManager class not found");
        return;
    }

    if (ObjectManager::Update_hook(hkObjectManagerUpdate)) {
        LOG_INFO("[ESP] ObjectManager::Update hooked");
    } else {
        LOG_ERROR("[ESP] Failed to hook ObjectManager::Update");
    }
}

void ESP::updateCache() {
    auto* mgr = cachedObjectManager;
    if (!mgr) return;

    static int heroOff = unity_detail::resolveFieldOffset(ObjectManager::uclass(), "m_Hero");
    if (heroOff <= 0) return;

    void* hero = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mgr) + heroOff);
    if (!hero) return;

    Vector3 hPos{};
    SGObject::GetPositionXYZ(hero, &hPos.x, &hPos.y, &hPos.z);

    int sw = UScreen::get_width();
    int sh = UScreen::get_height();
    if (sw <= 0 || sh <= 0) return;

    float maxDist = static_cast<float>(maxDistance);
    std::vector<EntityCache> tmp;
    tmp.reserve(64);

    auto collect = [&](int objectType, int typeTag) {
        void* listPtr = ObjectManager::GetObjectListByType(mgr, objectType);
        if (!listPtr) return;
        auto* list = reinterpret_cast<IList*>(listPtr);
        if (!list->pList || list->size <= 0 || list->size > 10000) return;

        bool wantName = static_cast<bool>(showName);

        for (int i = 0; i < list->size; i++) {
            void* obj = list->pList->At(i);
            if (!obj || obj == hero) continue;

            Vector3 pos{};
            SGObject::GetPositionXYZ(obj, &pos.x, &pos.y, &pos.z);

            float dx = pos.x - hPos.x;
            float dy = pos.y - hPos.y;
            float dz = pos.z - hPos.z;
            float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (dist > maxDist) continue;

            float sx, sy, sz;
            CameraUtility::CameraWorldToScreenPoint(pos.x, pos.y, pos.z, &sx, &sy, &sz);

            EntityCache ec{};
            ec.pos = pos;
            ec.dist = dist;
            ec.type = typeTag;
            ec.onScreen = (sz > 0.0f);
            ec.screenX = sx;
            ec.screenY = static_cast<float>(sh) - sy;

            if (wantName) {
                auto* ustr = UObject::ToString(obj);
                if (ustr && ustr->m_stringLength > 0)
                    ec.name = ustr->ToString();
            }

            tmp.push_back(std::move(ec));
        }
    };

    if (static_cast<bool>(showMonsters)) collect(ObjectType::Monster, 0);
    if (static_cast<bool>(showPlayers)) collect(ObjectType::Player, 1);
    if (static_cast<bool>(showNpcs)) collect(ObjectType::Npc, 2);

    std::lock_guard<std::mutex> lock(cacheMutex_);
    entities_.swap(tmp);
    heroPos_ = hPos;
    screenW_ = sw;
    screenH_ = sh;
}

void ESP::drawOverlay() {
    if (!isEnabled()) return;

    std::vector<EntityCache> snapshot;
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        snapshot = entities_;
    }

    if (snapshot.empty()) return;

    auto* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    auto& io = ImGui::GetIO();
    float sw = io.DisplaySize.x;
    float sh = io.DisplaySize.y;
    if (sw <= 0 || sh <= 0) return;

    ImVec2 screenBottom = {sw * 0.5f, sh};

    for (auto& e : snapshot) {
        if (!e.onScreen) continue;

        ImVec2 screenPos = {e.screenX, e.screenY};

        ImU32 color;
        if (e.type == 0)
            color = ImGui::ColorConvertFloat4ToU32({monsterColor[0], monsterColor[1], monsterColor[2], monsterColor[3]});
        else if (e.type == 1)
            color = ImGui::ColorConvertFloat4ToU32({playerColor[0], playerColor[1], playerColor[2], playerColor[3]});
        else
            color = ImGui::ColorConvertFloat4ToU32({npcColor[0], npcColor[1], npcColor[2], npcColor[3]});

        if (static_cast<bool>(showLine))
            dl->AddLine(screenBottom, screenPos, color, 1.0f);

        float textY = screenPos.y;

        if (static_cast<bool>(showName) && !e.name.empty()) {
            dl->AddText({screenPos.x + 4, textY}, color, e.name.c_str());
            textY += 14.0f;
        }

        if (static_cast<bool>(showDistance)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0fm", e.dist);
            dl->AddText({screenPos.x + 4, textY}, color, buf);
        }
    }
}

void ESP::dumpEntityInfo() {
    auto* mgr = cachedObjectManager;
    LOG_INFO("[ESP] === Dump Entity Info ===");
    LOG_INFO("[ESP] cachedObjectManager = {:p}", mgr ? mgr : (void*)0);

    if (!mgr) {
        LOG_ERROR("[ESP] No ObjectManager instance (hook not fired yet?)");
        return;
    }

    auto* klass = ObjectManager::uclass();
    if (!klass) {
        LOG_ERROR("[ESP] ObjectManager class is null");
        return;
    }

    LOG_INFO("[ESP] ObjectManager fields:");
    for (auto* f : klass->fields) {
        if (f) LOG_INFO("[ESP]   field: {} offset={}", f->name, f->offset);
    }

    static int heroOff = unity_detail::resolveFieldOffset(klass, "m_Hero");
    LOG_INFO("[ESP] m_Hero offset = {}", heroOff);

    if (heroOff > 0) {
        void* hero = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mgr) + heroOff);
        LOG_INFO("[ESP] hero ptr = {:p}", hero ? hero : (void*)0);
        if (hero) {
            Vector3 heroPos{};
            SGObject::GetPositionXYZ(hero, &heroPos.x, &heroPos.y, &heroPos.z);
            LOG_INFO("[ESP] hero pos=({:.2f}, {:.2f}, {:.2f})", heroPos.x, heroPos.y, heroPos.z);
        }
    }

    LOG_INFO("[ESP] Trying GetObjectListByType(Monster={})...", ObjectType::Monster);
    void* monsterList = ObjectManager::GetObjectListByType(mgr, ObjectType::Monster);
    LOG_INFO("[ESP] monsterList = {:p}", monsterList ? monsterList : (void*)0);
    if (monsterList) {
        auto* list = reinterpret_cast<IList*>(monsterList);
        LOG_INFO("[ESP]   list->size={} pList={:p}", list->size, (void*)list->pList);
        if (list->pList && list->size > 0) {
            for (int i = 0; i < list->size && i < 10; i++) {
                void* obj = list->pList->At(i);
                LOG_INFO("[ESP]     [{}] obj={:p}", i, obj ? obj : (void*)0);
                if (obj) {
                    Vector3 p{};
                    SGObject::GetPositionXYZ(obj, &p.x, &p.y, &p.z);
                    LOG_INFO("[ESP]         pos=({:.2f}, {:.2f}, {:.2f})", p.x, p.y, p.z);
                }
            }
        }
    }

    LOG_INFO("[ESP] === End Dump ===");
}

void ESP::drawUI() {
    if (Widgets::Section("esp", TR("ESP"))) {
        bool en = isEnabled();
        if (Widgets::Toggle(TR("Enable##esp"), &en))
            setEnabled(en);

        if (ImGui::Button("Dump Entity Info"))
            dumpEntityInfo();

        if (isEnabled()) {
            ImGui::Spacing();
            ImGui::SeparatorText(TR("Filters"));

            bool monsters = static_cast<bool>(showMonsters);
            if (Widgets::Toggle(TR("Monsters"), &monsters)) showMonsters = monsters;

            bool players = static_cast<bool>(showPlayers);
            if (Widgets::Toggle(TR("Players"), &players)) showPlayers = players;

            bool npcs = static_cast<bool>(showNpcs);
            if (Widgets::Toggle(TR("NPCs"), &npcs)) showNpcs = npcs;

            ImGui::Spacing();
            ImGui::SeparatorText(TR("Display"));

            bool line = static_cast<bool>(showLine);
            if (Widgets::Toggle(TR("Lines"), &line)) showLine = line;

            bool distance = static_cast<bool>(showDistance);
            if (Widgets::Toggle(TR("Distance"), &distance)) showDistance = distance;

            bool nameTgl = static_cast<bool>(showName);
            if (Widgets::Toggle(TR("Name"), &nameTgl)) showName = nameTgl;

            ImGui::Spacing();
            Widgets::SliderFloat(TR("Max Distance"), maxDistance, 10.0f, 500.0f);

            ImGui::Spacing();
            ImGui::SeparatorText(TR("Colors"));
            ImGui::ColorEdit4("Monster", monsterColor);
            ImGui::ColorEdit4("Player", playerColor);
            ImGui::ColorEdit4("NPC", npcColor);
        }

        Widgets::SectionEnd();
    }
}
