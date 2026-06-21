#pragma once

// ============================================================================
// Unity type declarations — include this to use any Unity class globally
// ============================================================================

using Vector3 = UnityResolve::UnityType::Vector3;
using UString = UnityResolve::UnityType::String;

// ---------------------------------------------------------------------------
// UnityEngine.CoreModule
// ---------------------------------------------------------------------------

class UApplication {
    UCLASS("UnityEngine.CoreModule.dll", "Application")
    UMETHOD(UString*, get_version)
    UMETHOD(void, set_targetFrameRate, int)
    UMETHOD(int, get_targetFrameRate)
};

class UQualitySettings {
    UCLASS("UnityEngine.CoreModule.dll", "QualitySettings")
    UMETHOD(void, set_vSyncCount, int)
    UMETHOD(int, get_vSyncCount)
};

class UTime {
    UCLASS("UnityEngine.CoreModule.dll", "Time")
    UMETHOD(float, get_deltaTime)
    UMETHOD(float, get_timeScale)
    UMETHOD(void, set_timeScale, float)
};

class UCamera {
    UCLASS("UnityEngine.CoreModule.dll", "Camera")
    UMETHOD(void*, get_main)
    UMETHOD(float, get_fieldOfView, void*)
    UMETHOD(void, set_fieldOfView, void*, float)
};

class UComponent {
    UCLASS("UnityEngine.CoreModule.dll", "Component")
    UMETHOD(void*, get_transform, void*)
    UMETHOD(void*, get_gameObject, void*)
};

class UTransform {
    UCLASS("UnityEngine.CoreModule.dll", "Transform")
    UMETHOD(void, get_position_Injected, void*, Vector3*)
    UMETHOD(void, set_position_Injected, void*, Vector3*)
    UMETHOD(void, get_localEulerAngles, void*, Vector3*)
    UMETHOD(void, set_localEulerAngles, void*, Vector3*)
    UMETHOD(void, get_localPosition_Injected, void*, Vector3*)
    UMETHOD(void, set_localPosition_Injected, void*, Vector3*)
    UMETHOD(void, get_localScale_Injected, void*, Vector3*)
    UMETHOD(void, set_localScale_Injected, void*, Vector3*)
};

class UGameObject {
    UCLASS("UnityEngine.CoreModule.dll", "GameObject")
    UMETHOD(void, SetActive, void*, bool)
    UMETHOD(bool, get_activeSelf, void*)
};

class UScreen {
    UCLASS("UnityEngine.CoreModule.dll", "Screen")
    UMETHOD(int, get_width)
    UMETHOD(int, get_height)
};
// ---------------------------------------------------------------------------
// IL2CPP Array (byte[])
// ---------------------------------------------------------------------------

struct Il2CppArray {
    void* klass;
    void* monitor;
    void* bounds;
    size_t max_length;
    uint8_t data[1];
};

// ---------------------------------------------------------------------------
// System.Reflection.Assembly (mscorlib)
// ---------------------------------------------------------------------------

class UAssembly {
    UCLASS("mscorlib.dll", "Assembly")
    UMETHOD(void*, Load, Il2CppArray*)
};

// ---------------------------------------------------------------------------
// HybridCLR.RuntimeApi
// ---------------------------------------------------------------------------

class URuntimeApi {
    UCLASS("HybridCLR.Runtime.dll", "RuntimeApi")
    UMETHOD(int, LoadMetadataForAOTAssembly, Il2CppArray*, int)
    UMETHOD(void, HotfixAssembly, void*, Il2CppArray*, void*)
    UMETHOD(int, LoadDifferentialHybridAssemblyWithDHAOImpl, Il2CppArray*, Il2CppArray*, Il2CppArray*)
    UMETHOD(int, LoadDifferentialHybridAssemblyWithMetaVersionImpl, Il2CppArray*, Il2CppArray*, Il2CppArray*, Il2CppArray*)
};
