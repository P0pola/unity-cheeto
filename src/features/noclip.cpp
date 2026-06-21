#include "pch.h"
#include "features/noclip.h"

static void hkSGHeroUpdate(void* self) {
    //__try {
        Noclip::Get().tickGameThread(self);
   // } __except(EXCEPTION_EXECUTE_HANDLER) {
   //     LOG_ERROR("[Noclip] SEH exception in tickGameThread");
   // }
	//LOG_DEBUG("[Noclip] hkSGHeroUpdate called for hero {:p}", self);
    SGHero::Update_original(self);
}

void Noclip::init() {
    LOG_INFO("[Noclip] init start");

    auto* klass = SGHero::uclass();
    if (!klass) {
        LOG_ERROR("[Noclip] SGHero class not found");
        return;
    }
   // LOG_INFO("[Noclip] SGHero class resolved: {}", klass->name);

    auto* addr = SGHero::Update_address();
    if (!addr) {
        LOG_ERROR("[Noclip] SGHero::Update address not found");
        return;
    }
    LOG_INFO("[Noclip] SGHero::Update address: {:p}", addr);

    if (SGHero::Update_hook(hkSGHeroUpdate)) {
        LOG_INFO("[Noclip] SGHero::Update hooked OK");
    } else {
        LOG_ERROR("[Noclip] Failed to hook SGHero::Update");
    }
}

void Noclip::onEnable() {
   // collisionDisabled_ = false;
   // LOG_INFO("[Noclip] Enabled");
}

void Noclip::onDisable() {
   // LOG_INFO("[Noclip] Disabling...");
   // collisionDisabled_ = false;
   // LOG_INFO("[Noclip] Disabled");
}

void Noclip::tickGameThread(void* hero) {
    if (!isEnabled()) {
        if (collisionDisabled_) {
            SGPlayer::SetCharacterControllerEnable(hero, true);
            collisionDisabled_ = false;
        }
        return;
    }

    if (!collisionDisabled_) {
        SGPlayer::SetCharacterControllerEnable(hero, false);
        collisionDisabled_ = true;
    }

    float px, py, pz;
    SGObject::GetPositionXYZ(hero, &px, &py, &pz);

    void* cam = UCamera::get_main();
    if (!cam) {
        LOG_ERROR("11");
        return;
    }
    void* camTransform = UComponent::get_transform(cam);
    if (!camTransform) {
        LOG_ERROR("22");
        return;
    } 

   // Quaternion rot{};
    //UTransform::get_rotation_Injected(camTransform, &rot);
    Vector3 forward = UTransform::get_forward(camTransform);
    Vector3 right = UTransform::get_right(camTransform);
    // forward = rot * (0,0,1)
    //Vector3 forward{
    //    2.0f * (rot.x * rot.z + rot.w * rot.y),
    //    2.0f * (rot.y * rot.z - rot.w * rot.x),
    //    1.0f - 2.0f * (rot.x * rot.x + rot.y * rot.y)
    //};
    // right = rot * (1,0,0)
    //Vector3 right{
    //    1.0f - 2.0f * (rot.y * rot.y + rot.z * rot.z),
    //    2.0f * (rot.x * rot.y + rot.w * rot.z),
    //    2.0f * (rot.x * rot.z - rot.w * rot.y)
    //};

    float speed = static_cast<float>(moveSpeed) * 0.016f;
    float mx = 0.0f, my = 0.0f, mz = 0.0f;

    auto key = HotkeyManager::isDown;
    if (key(forwardKey)) { mx += forward.x * speed; my += forward.y * speed; mz += forward.z * speed; }
    if (key(backKey))    { mx -= forward.x * speed; my -= forward.y * speed; mz -= forward.z * speed; }
    if (key(rightKey))   { mx += right.x * speed; my += right.y * speed; mz += right.z * speed; }
    if (key(leftKey))    { mx -= right.x * speed; my -= right.y * speed; mz -= right.z * speed; }
    if (key(upKey))      { my += speed; }
    if (key(downKey))    { my -= speed; }

    if (mx != 0.0f || my != 0.0f || mz != 0.0f) {
        SGObject::SetPositionXYZ(hero, px + mx, py + my, pz + mz, false);
    }
}

void Noclip::onTick() {
}

void Noclip::drawUI() {
    if (Widgets::Section("noclip", TR("Noclip"))) {
        bool en = isEnabled();
        if (Widgets::Toggle(TR("Enable##noclip"), &en))
            setEnabled(en);

        if (isEnabled()) {
            ImGui::Spacing();
            Widgets::SliderFloat(TR("Speed##noclip"), moveSpeed, 1.0f, 50.0f);

            ImGui::Spacing();
            ImGui::SeparatorText(TR("Bindings"));
            Widgets::KeyBind("Forward", forwardKey);
            Widgets::KeyBind("Back", backKey);
            Widgets::KeyBind("Left", leftKey);
            Widgets::KeyBind("Right", rightKey);
            Widgets::KeyBind("Up", upKey);
            Widgets::KeyBind("Down", downKey);
        }
        Widgets::SectionEnd();
    }
}
