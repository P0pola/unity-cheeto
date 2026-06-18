#pragma once

#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>
#include <magic_enum/magic_enum.hpp>
#include <imgui.h>

#include "vendor/xorstr.h"
#include "vendor/safetyhook/safetyhook.hpp"

// Vendor headers (depend on the above)
#include "vendor/Config/ConfigManager.h"
#include "vendor/Config/ConfigVar.h"
#include "vendor/logger/logger.h"
#include "vendor/HookManager/HookManager.h"
#include "vendor/Language/language.h"

// Project headers
#include "core/hotkey/hotkey_manager.h"
#include "vendor/UnityResolve.hpp"
#include "gui/widgets.h"
