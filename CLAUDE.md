# CLAUDE.md

本文件用于在此代码仓库中协助 Claude Code（claude.ai/code）开展工作，提供构建与代码结构方面的指引。

## 构建

使用 Visual Studio 2026（v145 工具集）打开 `UnityCheeto.sln`。解决方案包含两个项目：

- **UnityCheeto** — 输出 DLL（`bin/<Config>/UnityCheeto.dll`）。需要预编译头 `pch.h`。
- **Injector** — 控制台 EXE（`bin/Injector.exe`）。手动映射注入器，无外部依赖。

二者均仅支持 x64。DLL 链接 d3d12.lib、dxgi.lib、d3dcompiler.lib。注入器使用纯 Win32 API。



关键编译器选项：`/std:c++latest`、`/Zc:preprocessor`（unity_macros.h 中的 `__VA_OPT__` 需要此选项）、`/utf-8`。

## 架构

这是一个通过手动映射注入的 Unity 游戏 Mod DLL。整体流程如下：

1. **Injector**（`injector/`）轮询目标进程 → 手动映射 DLL（解析 PE、重定位、导入表、shellcode 调用 DllMain）
2. **DllMain**（`src/dllmain.cpp`）创建主线程 → 等待游戏模块（GameAssembly.dll 或 mono-2.0-bdwgc.dll）→ 初始化 UnityResolve → Hook DX12 → 注册功能模块
3. **DX12Hook**（`src/core/hooks/`）通过创建虚拟设备并抓取 VTable 来 Hook Present/ResizeBuffers/ExecuteCommandLists
4. **Renderer**（`src/renderer/`）管理 D3D12 资源与 ImGui 后端；`onPresent` 回调驱动每帧 tick 循环
5. **Features**（`src/features/`）每帧执行 tick 并绘制 UI

## 关键模式

### Unity 绑定（src/core/unity/）

在 `unity_types.h` 中使用宏声明 Unity 类：
```cpp
class UCamera {
    UCLASS("UnityEngine.CoreModule.dll", "Camera")
    UMETHOD(void*, get_main)
    UMETHOD(float, get_fieldOfView, void*)
    UMETHOD(void, set_fieldOfView, void*, float)
};
```

宏的变体：
- `UMETHOD(ret, name, ...)` — 静态方法，通过 UnityResolve 自动解析。会生成可调用的 `ClassName::name(args...)`、`name_address()`、`name_hook(detour)`、`name_original(args...)`
- `UFIELD(type, name, offset)` — 固定偏移，按值拷贝
- `UFIELD_AUTO(type, name, "fieldName")` — 运行时查找偏移
- `UFIELD_REF(type, name, offset)` / `UPROPERTY(type, name, "fieldName")` — 按引用返回
- `USTATIC_FIELD(type, name, "fieldName")` — 静态字段

### 功能系统

继承 `FeatureBase`（`src/features/feature_base.h`）：
```cpp
class MyFeature : public FeatureBase {
    ConfigVar<bool> enabled{"myfeature.enabled", false};
    void onEnable() override;
    void onDisable() override;
    void onTick() override;
    void drawUI() override;
};
```

在 dllmain.cpp 中通过 `GuiManager::addTab()` 注册。

### ConfigVar

`ConfigVar<T>`（vendor/Config/ConfigVar.h）封装了任意可 JSON 序列化类型，并自动持久化。批量修改可使用 `.Edit()` 的 RAII 代理。`src/gui/widgets.h` 中的控件可直接接收 ConfigVar。

### Hook

`HookManager::install(target, detour)` 使用 safetyhook 的内联 Hook。支持链式 Hook（同一目标可挂多个 detour，按 LIFO 顺序）。可通过 `HookManager::getInstance().getOriginal(detour)` 获取原始函数。

## 注入器配置

修改 `injector/main.cpp` 顶部：
```cpp
constexpr const wchar_t* TARGET_PROCESS = L"Game.exe";
constexpr const wchar_t* DLL_FILE       = L"UnityCheeto.dll";
```

## 重要说明

- DLL 采用手动映射（不是通过 LoadLibrary 加载），因此不得调用 `FreeLibraryAndExitThread`。从 DllMain 得到的模块句柄是有效的，但不会被系统加载器跟踪。
- `vendor/UnityResolve.hpp` 是较大的第三方头文件（约 2000 行）。不要修改它。
- 日志宏（`LOG_INFO`、`LOG_ERROR` 等）使用 `__VA_OPT__` —— 例如 `LOG_ERROR("msg")` 这种单参数调用是合法的。
- `external/imgui/` 下的 ImGui 源码作为 DLL 项目的一部分编译，并设置为不使用预编译头（NotUsing）。
