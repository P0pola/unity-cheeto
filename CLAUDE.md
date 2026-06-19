# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 构建

使用 Visual Studio 2022/2026（v145 工具集）打开 `UnityCheeto.sln`。解决方案包含两个项目：

- **UnityCheeto** — DLL 输出到 `bin/<Config>/UnityCheeto.dll`，需要预编译头 `pch.h`
- **Injector** — 控制台 EXE 输出到 `bin/Injector.exe`，手动映射注入器

仅支持 x64。关键编译选项：`/std:c++latest`、`/Zc:preprocessor`（`__VA_OPT__` 依赖）、`/utf-8`。

DLL 链接库：dxgi.lib、d3dcompiler.lib、opengl32.lib。

### 图形 API 切换

在 `src/pch.h` 第 9 行修改宏：
```cpp
#define USE_GFX_API GFX_API_DX11   // GFX_API_DX11 | GFX_API_DX12 | GFX_API_OPENGL
```

## 架构

注入式 Unity 游戏 Mod DLL。执行流程：

1. **Injector** 读取 `injector.ini` 中的 ExePath → 启动游戏进程 → 手动映射 DLL
2. **DllMain** → 主线程等待 GameAssembly.dll/mono-2.0-bdwgc.dll → UnityResolve 初始化
3. **App::initialize()** → `FeatureBase::initAll()` 自动初始化所有已注册 feature → 注册 GUI tabs
4. **Hook 回调**（onPresent/onSwapBuffers）驱动每帧：`App::tick()` → `FeatureBase::tickAll()` → Renderer::render()

### 模块结构

- `src/dllmain.cpp` — DLL 入口 + hook 回调（薄层）
- `src/app.h/cpp` — 应用层：feature 注册触发、GUI tabs、toggle key
- `src/features/` — 功能模块，自动注册
- `src/core/hooks/` — DX11/DX12/OpenGL hook
- `src/core/unity/` — Unity 绑定宏和类型声明
- `src/renderer/` — DX11/DX12/OpenGL 渲染器（ImGui 后端）
- `src/gui/` — GuiManager（tab 系统）+ Widgets（自定义控件）
- `vendor/` — UnityResolve、safetyhook、ConfigManager、Logger、Language

## Feature 自动注册系统

继承 `FeatureBase`，实现必要虚函数，singleton 构造时自动注册到全局 registry：

```cpp
// my_feature.h
class MyFeature : public FeatureBase {
public:
    static MyFeature& Get() { static MyFeature inst; return inst; }

    void init() override;       // hook 安装等一次性初始化
    void onEnable() override;   // toggle 开启时调用
    void onDisable() override;  // toggle 关闭时调用
    void onTick() override;     // 每帧调用
    void drawUI() override;     // ImGui 绘制

    const char* name() const override { return "My Feature"; }
    const char* category() const override { return "World"; } // 决定显示在哪个 tab

    ConfigVar<float> someVal{"myfeature.value", 1.0f};

private:
    MyFeature() : FeatureBase("myfeature") {}
};
```

```cpp
// my_feature.cpp — 底部触发注册
static auto& _reg = MyFeature::Get();
```

在 `src/app.cpp` 顶部也需加一行 `static auto& _xxx = MyFeature::Get();` 确保链接。

category 可选值对应 GUI tabs：`"Player"`、`"World"`、`"Visual"`、`"Misc"`。

## Unity 绑定（unity_types.h + unity_macros.h）

### 声明 Unity 类

```cpp
class UTime {
    UCLASS("UnityEngine.CoreModule.dll", "Time")
    UMETHOD(float, get_deltaTime)
    UMETHOD(void, set_timeScale, float)
};
```

`UCLASS(dll名, 类名)` 通过 UnityResolve 解析出 Class 指针。

### UMETHOD — 声明方法

```cpp
UMETHOD(返回类型, 方法名, 参数类型...)
```

自动生成四个接口：
- `UTime::set_timeScale(1.0f)` — 直接调用
- `UTime::set_timeScale_address()` — 获取函数地址 (void*)
- `UTime::set_timeScale_hook(detour)` — 安装 inline hook
- `UTime::set_timeScale_original(args...)` — 调用 hook 前的原始函数

### Hook 示例

```cpp
void WorldSpeed::init() {
    UTime::set_timeScale_hook([](float value) {
        if (WorldSpeed::Get().isEnabled())
            value = static_cast<float>(WorldSpeed::Get().speed);
        UTime::set_timeScale_original(value);
    });
}
```

### 字段宏

- `UFIELD(type, name, offset)` — 固定偏移量读写（返回值拷贝）
- `UFIELD_REF(type, name, offset)` — 固定偏移量引用
- `UFIELD_AUTO(type, name, "fieldStr")` — 运行时按名字查找偏移
- `UPROPERTY(type, name, "fieldStr")` — 运行时查找，返回引用
- `USTATIC_FIELD(type, name, "fieldStr")` — 静态字段读写

### 类解析变体

- `UCLASS(module, className)` — 按名字解析
- `UCLASS_FROM_FIELD(module, container, fieldName)` — 从另一个类的字段类型解析（适用于嵌套类型）
- `UCLASS_FROM_FIELDS(module, minMatches, "field1", "field2", ...)` — 按字段特征匹配（适用于混淆类名）

### 实例方法 vs 静态方法

Unity C# 的实例方法在 IL2CPP 中第一个参数是 `this` 指针：
```cpp
class UComponent {
    UCLASS("UnityEngine.CoreModule.dll", "Component")
    UMETHOD(void*, get_transform, void*)  // void* this
};
// 调用：UComponent::get_transform(instance)
```

静态方法没有 this 参数：
```cpp
class UCamera {
    UCLASS("UnityEngine.CoreModule.dll", "Camera")
    UMETHOD(void*, get_main)  // 无参数
};
// 调用：UCamera::get_main()
```

## ConfigVar

`ConfigVar<T>` 自动持久化到 JSON。Widgets 直接接受 ConfigVar 引用：
```cpp
ConfigVar<float> speed{"myfeature.speed", 1.0f};
Widgets::SliderFloat("Speed", speed, 0.1f, 10.0f);
```

批量修改用 `.Edit()` RAII 代理避免多次写盘。

## HookManager

```cpp
HookManager::install(target_fn_ptr, detour_fn_ptr);
auto original = HookManager::getInstance().getOriginal(detour_fn_ptr);
```

基于 safetyhook 内联 hook。支持链式 hook（同一目标多个 detour，LIFO 顺序）。UMETHOD 宏已封装此流程，通常直接用 `ClassName::method_hook()` 即可。

## 重要说明

- DLL 是手动映射的，不得调用 `FreeLibraryAndExitThread`
- `vendor/UnityResolve.hpp` 约 2000 行，不要修改
- 日志宏（LOG_INFO、LOG_ERROR 等）使用 `__VA_OPT__`，单参数合法
- `external/imgui/` 源码编译时不使用预编译头（NotUsing）
- 新增 feature 不需要修改 dllmain.cpp，只需写 .h/.cpp + 在 app.cpp 加注册行
