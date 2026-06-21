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
// SGEngine.Common.Runtime.dll — Singleton<T>
// ---------------------------------------------------------------------------

// SGEngine.Core.Singleton<T> has a static field "m_Instance"
// Access via: ObjectManager class → USTATIC_FIELD

// ---------------------------------------------------------------------------
// Assembly-CSharp.dll — SGEngine game types
// ---------------------------------------------------------------------------

class SGObject {
    UCLASS("Assembly-CSharp.dll", "SGObject")
    UMETHOD(Vector3, GetPosition, void*)
    UMETHOD(void, GetPositionXYZ, void*, float*, float*, float*)
    UMETHOD(float, GetHeight, void*)
    UMETHOD(float, GetRadius, void*)
    UMETHOD(bool, IsValid, void*)
    UMETHOD(bool, IsActive, void*)

    UPROPERTY(long long, ObjectID, "m_ObjectID")
    UPROPERTY(void*, Transform, "m_Transform")
    UPROPERTY(Vector3, Position, "m_Position")
    UPROPERTY(bool, IsDestroyed, "m_IsDestroyed")
    UPROPERTY(float, Height, "m_Height")
    UPROPERTY(float, Radius, "m_Radius")
};

class ObjectManager {
    UCLASS("Assembly-CSharp.dll", "ObjectManager")
    UMETHOD(void, Update, void*)
    UMETHOD(void*, GetObjectMap, void*)
    UMETHOD(void*, GetObjectListByType, void*, int)
    UMETHOD(void*, GetObject, void*, long long)

    UPROPERTY(void*, Hero, "m_Hero")
    UPROPERTY(void*, ObjectMap, "m_ObjectMap")
    UPROPERTY(void*, ObjectTypeMap, "m_ObjectTypeMap")


};

class CameraUtility {
    UCLASS("Assembly-CSharp.dll", "CameraUtility")
    UMETHOD(void, CameraWorldToScreenPoint, float, float, float, float*, float*, float*)
};

// ObjectType enum values
namespace ObjectType {
    constexpr int Hero = 8;
    constexpr int Player = 128;
    constexpr int Monster = 256;
    constexpr int Npc = 512;
    constexpr int Pet = 1024;
    constexpr int Puppet = 131072;
}
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
// System.Object (mscorlib) — for ToString
// ---------------------------------------------------------------------------

class UObject {
    UCLASS("mscorlib.dll", "Object")
    UMETHOD(UString*, ToString, void*)
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
