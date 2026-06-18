#include "pch.h"
#include "features/free_camera.h"

void FreeCamera::onEnable() {
    LOG_INFO("Free Camera enabled");
    GetCursorPos(&lastMouse_);
    yaw_ = 0.0f;
    pitch_ = 0.0f;
    cameraObj_ = nullptr;
}

void FreeCamera::onDisable() {
    LOG_INFO("Free Camera disabled");
    cameraObj_ = nullptr;
}

void FreeCamera::onTick() {
    if (!isEnabled()) return;

    auto* assembly = UnityResolve::Get("UnityEngine.CoreModule.dll");
    if (!assembly) assembly = UnityResolve::Get("UnityEngine.dll");
    if (!assembly) return;

    auto* cameraClass = assembly->Get("Camera");
    if (!cameraClass) return;

    if (!cameraObj_) {
        auto* mainGetter = cameraClass->Get<UnityResolve::Method>("get_main");
        if (!mainGetter) return;
        cameraObj_ = mainGetter->Invoke<void*>();
        if (!cameraObj_) return;
    }

    auto* componentClass = assembly->Get("Component");
    if (!componentClass) return;
    auto* getTransform = componentClass->Get<UnityResolve::Method>("get_transform");
    if (!getTransform) return;

    void* transform = getTransform->Invoke<void*>(cameraObj_);
    if (!transform) return;

    auto* transformClass = assembly->Get("Transform");
    if (!transformClass) return;

    POINT currentMouse;
    GetCursorPos(&currentMouse);
    float dx = static_cast<float>(currentMouse.x - lastMouse_.x);
    float dy = static_cast<float>(currentMouse.y - lastMouse_.y);
    lastMouse_ = currentMouse;

    float sens = sensitivity;
    if (HotkeyManager::isDown(VK_RBUTTON)) {
        yaw_ += dx * sens * 0.1f;
        pitch_ -= dy * sens * 0.1f;
        pitch_ = std::clamp(pitch_, -89.0f, 89.0f);

        auto* setEuler = transformClass->Get<UnityResolve::Method>("set_localEulerAngles_Injected");
        if (setEuler) {
            UnityResolve::UnityType::Vector3 euler{pitch_, yaw_, 0.0f};
            setEuler->Invoke<void>(transform, &euler);
        }
    }

    float yawRad = yaw_ * 3.14159265f / 180.0f;
    float pitchRad = pitch_ * 3.14159265f / 180.0f;

    UnityResolve::UnityType::Vector3 forward{
        std::cos(pitchRad) * std::sin(yawRad),
        -std::sin(pitchRad),
        std::cos(pitchRad) * std::cos(yawRad)
    };
    UnityResolve::UnityType::Vector3 right{
        std::cos(yawRad),
        0.0f,
        -std::sin(yawRad)
    };

    float speed = static_cast<float>(moveSpeed) * 0.016f;
    UnityResolve::UnityType::Vector3 move{0.0f, 0.0f, 0.0f};

    auto key = HotkeyManager::isDown;
    if (key(forwardKey)) { move.x += forward.x * speed; move.y += forward.y * speed; move.z += forward.z * speed; }
    if (key(backKey))    { move.x -= forward.x * speed; move.y -= forward.y * speed; move.z -= forward.z * speed; }
    if (key(rightKey))   { move.x += right.x * speed; move.y += right.y * speed; move.z += right.z * speed; }
    if (key(leftKey))    { move.x -= right.x * speed; move.y -= right.y * speed; move.z -= right.z * speed; }
    if (key(upKey))      { move.y += speed; }
    if (key(downKey))    { move.y -= speed; }

    if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f) {
        auto* getPos = transformClass->Get<UnityResolve::Method>("get_position_Injected");
        auto* setPos = transformClass->Get<UnityResolve::Method>("set_position_Injected");
        if (getPos && setPos) {
            UnityResolve::UnityType::Vector3 pos{};
            getPos->Invoke<void>(transform, &pos);
            pos.x += move.x;
            pos.y += move.y;
            pos.z += move.z;
            setPos->Invoke<void>(transform, &pos);
        }
    }
}

void FreeCamera::drawUI() {
    Widgets::Toggle("Enable##cam", enabled_);

    if (!isEnabled()) return;

    ImGui::Spacing();
    Widgets::SliderFloat("Move Speed", moveSpeed, 1.0f, 100.0f);
    Widgets::SliderFloat("Sensitivity", sensitivity, 0.1f, 10.0f);

    ImGui::Spacing();
    ImGui::SeparatorText("Bindings");
    Widgets::KeyBind("Forward", forwardKey);
    Widgets::KeyBind("Back", backKey);
    Widgets::KeyBind("Left", leftKey);
    Widgets::KeyBind("Right", rightKey);
    Widgets::KeyBind("Up", upKey);
    Widgets::KeyBind("Down", downKey);

    ImGui::Spacing();
    ImGui::TextDisabled("RMB to rotate");
}
