#pragma once
#include <imgui.h>
#include <string>

namespace Widgets {

bool Toggle(const char* label, bool* v);
bool SliderFloat(const char* label, float* v, float min, float max, const char* fmt = "%.1f");
bool SliderInt(const char* label, int* v, int min, int max);
bool KeyBind(const char* label, int* key);
bool CategoryHeader(const char* label, bool* open);
void HelpMarker(const char* desc);

// ConfigVar overloads
bool Toggle(const char* label, ConfigVar<bool>& cv);
bool SliderFloat(const char* label, ConfigVar<float>& cv, float min, float max, const char* fmt = "%.1f");
bool SliderInt(const char* label, ConfigVar<int>& cv, int min, int max);
bool KeyBind(const char* label, ConfigVar<int>& cv);

} // namespace Widgets
