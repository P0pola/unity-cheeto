#pragma once

// Requires: UnityResolve.hpp, HookManager.h, Logger (via pch.h)

// ============================================================================
// Unity Type Macros - Simplified class/method/field declaration system
// ============================================================================
//
// Usage:
//   class Player {
//       UCLASS("Assembly-CSharp.dll", "Player")
//       UMETHOD(void, Update, Player*)
//       UMETHOD(float, get_Health, Player*)
//       UFIELD(float, health, 0x48)
//       UFIELD_AUTO(int, level, "m_level")
//   };
//
//   // Call a method:
//   Player::Update(instance);
//
//   // Hook a method:
//   Player::Update_hook([](Player* self) {
//       Player::Update_original(self);
//       // post-hook logic
//   });
//
//   // Read/write a field:
//   float hp = instance->health();
//   instance->health(100.0f);
//

using UTYPE = UnityResolve::UnityType;

// ============================================================================
// Internal helpers
// ============================================================================

namespace unity_detail {

inline UnityResolve::Class* resolveClass(const char* module, const char* name) {
    auto* mod = UnityResolve::Get(module);
    if (!mod) {
        LOG_ERROR("[Unity] Module '{}' not found", module);
        return nullptr;
    }
    auto* klass = mod->Get(name);
    if (!klass) {
        LOG_ERROR("[Unity] Class '{}' not found in '{}'", name, module);
        return nullptr;
    }
    return klass;
}

inline UnityResolve::Class* resolveClassFromField(const char* module,
                                                   const char* container,
                                                   const char* fieldName) {
    auto* klass = resolveClass(module, container);
    if (!klass) return nullptr;

    for (auto* field : klass->fields) {
        if (!field || !field->type || field->name != fieldName) continue;
        const auto& typeName = field->type->name;
        auto shortName = typeName.substr(typeName.find_last_of('.') + 1);
        return resolveClass(module, shortName.c_str());
    }
    LOG_ERROR("[Unity] Field '{}' not found in '{}'", fieldName, container);
    return nullptr;
}

inline UnityResolve::Class* resolveClassFromFields(const char* module,
                                                    std::initializer_list<const char*> fields,
                                                    size_t minMatches = 0) {
    auto* mod = UnityResolve::Get(module);
    if (!mod) return nullptr;

    if (minMatches == 0) minMatches = fields.size();

    for (auto* klass : mod->classes) {
        if (!klass || klass->fields.empty()) continue;
        size_t matched = 0;
        for (auto* req : fields) {
            for (auto* f : klass->fields) {
                if (f && f->name == req) { matched++; break; }
            }
        }
        if (matched >= minMatches) return klass;
    }
    LOG_ERROR("[Unity] No class matched field set in '{}'", module);
    return nullptr;
}

inline UnityResolve::Method* resolveMethod(UnityResolve::Class* klass, const char* name) {
    if (!klass) return nullptr;
    auto* m = klass->Get<UnityResolve::Method>(name);
    if (!m) {
        LOG_ERROR("[Unity] Method '{}' not found in '{}'", name, klass->name);
    }
    return m;
}

inline int resolveFieldOffset(UnityResolve::Class* klass, const char* name) {
    if (!klass) return -1;
    for (auto* f : klass->fields) {
        if (f && f->name == name) return f->offset;
    }
    LOG_ERROR("[Unity] Field '{}' not found in '{}'", name, klass->name);
    return -1;
}

} // namespace unity_detail

// ============================================================================
// UCLASS — Declare a Unity class binding
// ============================================================================

#define UCLASS(MODULE, CLASS_NAME) \
public: \
    static UnityResolve::Class* uclass() { \
        static auto* c = unity_detail::resolveClass(MODULE, CLASS_NAME); \
        return c; \
    } \
    static constexpr const char* uclass_module() { return MODULE; } \
    static constexpr const char* uclass_name() { return CLASS_NAME; }

// Resolve class by finding it as a field type in another class
#define UCLASS_FROM_FIELD(MODULE, CONTAINER, FIELD_NAME) \
public: \
    static UnityResolve::Class* uclass() { \
        static auto* c = unity_detail::resolveClassFromField(MODULE, CONTAINER, FIELD_NAME); \
        return c; \
    } \
    static constexpr const char* uclass_module() { return MODULE; }

// Resolve class by matching field names (for obfuscated classes)
#define UCLASS_FROM_FIELDS(MODULE, MIN_MATCHES, ...) \
public: \
    static UnityResolve::Class* uclass() { \
        static auto* c = unity_detail::resolveClassFromFields(MODULE, { __VA_ARGS__ }, MIN_MATCHES); \
        return c; \
    } \
    static constexpr const char* uclass_module() { return MODULE; }

// ============================================================================
// UMETHOD — Declare a callable + hookable Unity method
// ============================================================================
//
// Generates:
//   static ReturnType MethodName(Args...)        — calls the method
//   static void MethodName_hook(detour_fn)       — installs a hook
//   static ReturnType MethodName_original(Args...)— calls the original (pre-hook)
//   static void* MethodName_address()            — raw address
//

#define UMETHOD(RETURN_TYPE, METHOD_NAME, ...) \
private: \
    struct METHOD_NAME##_t { \
        using fn_t = RETURN_TYPE(*)(__VA_ARGS__); \
        static UnityResolve::Method* resolve() { \
            static auto* m = unity_detail::resolveMethod(uclass(), #METHOD_NAME); \
            return m; \
        } \
        static fn_t get() { \
            static fn_t ptr = [] { \
                auto* m = resolve(); \
                return m ? m->Cast<RETURN_TYPE __VA_OPT__(,) __VA_ARGS__>() : (fn_t)nullptr; \
            }(); \
            return ptr; \
        } \
        static inline std::unordered_map<void*, fn_t> detour_originals; \
    }; \
public: \
    template<typename... Args> \
    static RETURN_TYPE METHOD_NAME(Args&&... args) { \
        auto fn = METHOD_NAME##_t::get(); \
        if (fn) return fn(std::forward<Args>(args)...); \
        if constexpr (!std::is_void_v<RETURN_TYPE>) { RETURN_TYPE _{}; return _; } \
    } \
    static void* METHOD_NAME##_address() { \
        auto* m = METHOD_NAME##_t::resolve(); \
        return m ? *static_cast<void**>(m->address) : nullptr; \
    } \
    static bool METHOD_NAME##_hook(typename METHOD_NAME##_t::fn_t detour) { \
        auto* addr = METHOD_NAME##_address(); \
        if (!addr) { \
            LOG_ERROR("[Unity] Cannot hook '{}' - address not found", #METHOD_NAME); \
            return false; \
        } \
        bool ok = HookManager::install( \
            reinterpret_cast<typename METHOD_NAME##_t::fn_t>(addr), detour); \
        if (ok) { \
            METHOD_NAME##_t::detour_originals[reinterpret_cast<void*>(detour)] = \
                HookManager::getInstance().getOriginal(detour); \
        } \
        return ok; \
    } \
    template<typename... Args> \
    __declspec(noinline) \
    static RETURN_TYPE METHOD_NAME##_original(Args&&... args) { \
        void* retAddr = _ReturnAddress(); \
        typename METHOD_NAME##_t::fn_t best_original = nullptr; \
        void* best_detour = nullptr; \
        for (auto& [detour, orig] : METHOD_NAME##_t::detour_originals) { \
            if (detour <= retAddr && (best_detour == nullptr || detour > best_detour)) { \
                best_detour = detour; \
                best_original = orig; \
            } \
        } \
        if (best_original) \
            return best_original(std::forward<Args>(args)...); \
        if constexpr (!std::is_void_v<RETURN_TYPE>) { RETURN_TYPE _{}; return _; } \
    }

// ============================================================================
// UMETHOD_OVERLOAD — For overloaded methods (specify parameter types to disambiguate)
// ============================================================================

#define UMETHOD_OVERLOAD(RETURN_TYPE, METHOD_NAME, PARAM_TYPES, ...) \
private: \
    struct METHOD_NAME##_##PARAM_TYPES##_t { \
        using fn_t = RETURN_TYPE(*)(__VA_ARGS__); \
        static fn_t get() { \
            static fn_t ptr = [] { \
                auto* klass = uclass(); \
                if (!klass) return (fn_t)nullptr; \
                auto* m = klass->Get<UnityResolve::Method>(#METHOD_NAME, #PARAM_TYPES); \
                return m ? m->Cast<RETURN_TYPE __VA_OPT__(,) __VA_ARGS__>() : (fn_t)nullptr; \
            }(); \
            return ptr; \
        } \
    }; \
public: \
    template<typename... Args> \
    static RETURN_TYPE METHOD_NAME##_##PARAM_TYPES(Args&&... args) { \
        auto fn = METHOD_NAME##_##PARAM_TYPES##_t::get(); \
        if (fn) return fn(std::forward<Args>(args)...); \
        if constexpr (!std::is_void_v<RETURN_TYPE>) { RETURN_TYPE _{}; return _; } \
    }

// ============================================================================
// UFIELD — Fixed-offset field (when you know the offset at compile time)
// ============================================================================
//
// Generates getter/setter:
//   Type name()           — read
//   void name(Type val)   — write
//

#define UFIELD(TYPE, NAME, OFFSET) \
public: \
    TYPE NAME() const { \
        return *reinterpret_cast<TYPE*>(reinterpret_cast<uintptr_t>(this) + (OFFSET)); \
    } \
    void NAME(TYPE value) { \
        *reinterpret_cast<TYPE*>(reinterpret_cast<uintptr_t>(this) + (OFFSET)) = value; \
    }

// ============================================================================
// UFIELD_AUTO — Dynamic offset resolution (resolved once at first access)
// ============================================================================
//
// For obfuscated fields: provide a friendly name + the real field name string
//   UFIELD_AUTO(float, health, "m_Health")
//   UFIELD_AUTO(int, level, "<Level>k__BackingField")
//

#define UFIELD_AUTO(TYPE, NAME, FIELD_STR) \
public: \
    TYPE NAME() const { \
        static int off = unity_detail::resolveFieldOffset(uclass(), FIELD_STR); \
        if (off < 0) return TYPE{}; \
        return *reinterpret_cast<TYPE*>(reinterpret_cast<uintptr_t>(this) + off); \
    } \
    void NAME(TYPE value) { \
        static int off = unity_detail::resolveFieldOffset(uclass(), FIELD_STR); \
        if (off < 0) return; \
        *reinterpret_cast<TYPE*>(reinterpret_cast<uintptr_t>(this) + off) = value; \
    }

// ============================================================================
// UFIELD_REF — Returns a reference (for large types or when you need &)
// ============================================================================

#define UFIELD_REF(TYPE, NAME, OFFSET) \
public: \
    TYPE& NAME() { \
        return *reinterpret_cast<TYPE*>(reinterpret_cast<uintptr_t>(this) + (OFFSET)); \
    } \
    const TYPE& NAME() const { \
        return *reinterpret_cast<const TYPE*>(reinterpret_cast<uintptr_t>(this) + (OFFSET)); \
    }

// ============================================================================
// UPROPERTY — Auto-resolve field, returns reference (best for structs/vectors)
// ============================================================================

#define UPROPERTY(TYPE, NAME, FIELD_STR) \
public: \
    TYPE& NAME() { \
        static int off = unity_detail::resolveFieldOffset(uclass(), FIELD_STR); \
        return *reinterpret_cast<TYPE*>(reinterpret_cast<uintptr_t>(this) + off); \
    }

// ============================================================================
// USTATIC_FIELD — Access a static field value
// ============================================================================

#define USTATIC_FIELD(TYPE, NAME, FIELD_STR) \
public: \
    static TYPE NAME() { \
        auto* klass = uclass(); \
        if (!klass) return TYPE{}; \
        for (auto* f : klass->fields) { \
            if (f && f->name == FIELD_STR && f->static_field) { \
                return *reinterpret_cast<TYPE*>(f->static_field); \
            } \
        } \
        return TYPE{}; \
    } \
    static void NAME(TYPE value) { \
        auto* klass = uclass(); \
        if (!klass) return; \
        for (auto* f : klass->fields) { \
            if (f && f->name == FIELD_STR && f->static_field) { \
                *reinterpret_cast<TYPE*>(f->static_field) = value; \
                return; \
            } \
        } \
    }
