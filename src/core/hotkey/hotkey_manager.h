#pragma once
#include <functional>
#include <unordered_map>
#include <Windows.h>

class HotkeyManager {
public:
    static HotkeyManager& Get() { static HotkeyManager inst; return inst; }

    using Callback = std::function<void()>;

    void onPress(int vkey, Callback cb) { press_[vkey] = std::move(cb); }
    void onRelease(int vkey, Callback cb) { release_[vkey] = std::move(cb); }
    void unbind(int vkey) { press_.erase(vkey); release_.erase(vkey); }

    static bool isDown(int vkey) { return (GetAsyncKeyState(vkey) & 0x8000) != 0; }

    void tick() {
        for (auto& [vkey, cb] : press_) {
            bool down = (GetAsyncKeyState(vkey) & 0x8000) != 0;
            bool wasDown = state_[vkey];
            if (down && !wasDown) cb();
            state_[vkey] = down;
        }
        for (auto& [vkey, cb] : release_) {
            bool down = (GetAsyncKeyState(vkey) & 0x8000) != 0;
            bool wasDown = state_[vkey];
            if (!down && wasDown) cb();
            state_[vkey] = down;
        }
    }

private:
    HotkeyManager() = default;
    std::unordered_map<int, Callback> press_;
    std::unordered_map<int, Callback> release_;
    std::unordered_map<int, bool> state_;
};
