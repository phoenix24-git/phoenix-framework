// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Component type registry + reflection, built on the Phoenix foundation layer (phoenix_core):
// components use foundation types (Vector3, Quaternion, Color, Transform3D, String, Vector<T>),
// and every reflected field converts to/from a foundation Variant. That one mechanism powers
// serialization (JSON/binary via Variant), prefab overrides, the editor inspector and C#.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <typeinfo>

#include "core/math/color.h"
#include "core/math/quaternion.h"
#include "core/math/transform_3d.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "phoenix/ecs/entity.h"

namespace phoenix::ecs {

using ComponentId = std::uint32_t;
inline constexpr ComponentId kInvalidComponent = ~0u;

/// Reference to an imported asset (mesh, material, clip, …) by stable UID (ResourceUID).
struct AssetRef {
    std::int64_t uid = 0;
    bool operator==(const AssetRef& o) const { return uid == o.uid; }
};

/// Bit mask over the 32 layers (Unity's LayerMask).
struct LayerMask {
    std::uint32_t value = ~0u;
    bool operator==(const LayerMask& o) const { return value == o.value; }
};

enum class FieldKind : std::uint8_t {
    Bool,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Float,
    Double,
    Vector2,
    Vector3,
    Vector4,
    Quaternion,
    Color,
    Transform3D,
    Entity,
    Asset,
    LayerMask,
    Enum,
    String,
    StringName,
    FloatArray,
    Vector3Array,
    AssetArray,
    EntityArray,
};

const char* field_kind_name(FieldKind kind);

struct FieldInfo {
    const char* name;
    FieldKind kind;
    std::uint32_t offset;
    std::uint32_t size;
    /// For enums: the enum's value names in declaration order (nullptr-terminated), else null.
    const char* const* enum_names = nullptr;
};

/// How far a component's behaviour is implemented (its data always is).
enum class ComponentStatus : std::uint8_t {
    Simulated,  ///< data + a system implementing the behaviour
    DataOnly,   ///< data, reflection, serialization; the behaviour lands with its server
};

struct ComponentInfo {
    ComponentId id = kInvalidComponent;
    String name;        ///< Phoenix type name, e.g. "Rigidbody"
    String unity_name;  ///< Unity Inspector name it corresponds to
    String category;    ///< Unity "Add Component" category
    String doc;
    ComponentStatus status = ComponentStatus::DataOnly;
    std::uint32_t size = 0;  ///< 0 for tag components (no storage)
    std::uint32_t align = 1;
    void (*construct)(void* dst) = nullptr;
    void (*destroy)(void* p) = nullptr;
    void (*move_construct)(void* dst, void* src) = nullptr;
    void (*copy_construct)(void* dst, const void* src) = nullptr;
    LocalVector<FieldInfo> fields;
    const std::type_info* type = nullptr;

    const FieldInfo* field(const char* field_name) const;
    /// Reads a field as a foundation Variant (Entity/AssetRef/LayerMask/enum -> int).
    Variant get(const void* component, const FieldInfo& f) const;
    /// Writes a field from a Variant. Returns false on a type mismatch.
    bool set(void* component, const FieldInfo& f, const Variant& value) const;
    /// All fields as a Dictionary {field name: value}; the basis of serialization and prefabs.
    Dictionary to_dictionary(const void* component) const;
    /// Applies the keys present in `d` (unknown keys are ignored; returns the count applied).
    int from_dictionary(void* component, const Dictionary& d) const;
};

namespace detail {

template <typename T>
constexpr FieldKind field_kind() {
    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<U, bool>)
        return FieldKind::Bool;
    else if constexpr (std::is_enum_v<U>)
        return FieldKind::Enum;
    else if constexpr (std::is_same_v<U, std::int32_t>)
        return FieldKind::Int32;
    else if constexpr (std::is_same_v<U, std::uint32_t>)
        return FieldKind::UInt32;
    else if constexpr (std::is_same_v<U, std::int64_t>)
        return FieldKind::Int64;
    else if constexpr (std::is_same_v<U, std::uint64_t>)
        return FieldKind::UInt64;
    else if constexpr (std::is_same_v<U, float>)
        return FieldKind::Float;
    else if constexpr (std::is_same_v<U, double>)
        return FieldKind::Double;
    else if constexpr (std::is_same_v<U, ::Vector2>)
        return FieldKind::Vector2;
    else if constexpr (std::is_same_v<U, ::Vector3>)
        return FieldKind::Vector3;
    else if constexpr (std::is_same_v<U, ::Vector4>)
        return FieldKind::Vector4;
    else if constexpr (std::is_same_v<U, ::Quaternion>)
        return FieldKind::Quaternion;
    else if constexpr (std::is_same_v<U, ::Color>)
        return FieldKind::Color;
    else if constexpr (std::is_same_v<U, ::Transform3D>)
        return FieldKind::Transform3D;
    else if constexpr (std::is_same_v<U, Entity>)
        return FieldKind::Entity;
    else if constexpr (std::is_same_v<U, AssetRef>)
        return FieldKind::Asset;
    else if constexpr (std::is_same_v<U, LayerMask>)
        return FieldKind::LayerMask;
    else if constexpr (std::is_same_v<U, ::String>)
        return FieldKind::String;
    else if constexpr (std::is_same_v<U, ::StringName>)
        return FieldKind::StringName;
    else if constexpr (std::is_same_v<U, ::Vector<float>>)
        return FieldKind::FloatArray;
    else if constexpr (std::is_same_v<U, ::Vector<::Vector3>>)
        return FieldKind::Vector3Array;
    else if constexpr (std::is_same_v<U, ::Vector<AssetRef>>)
        return FieldKind::AssetArray;
    else if constexpr (std::is_same_v<U, ::Vector<Entity>>)
        return FieldKind::EntityArray;
    else
        static_assert(sizeof(U) == 0, "unsupported component field type");
}

template <typename E>
constexpr bool valid_enum() {
    if constexpr (std::is_enum_v<E>)
        return sizeof(E) == 4;
    else
        return true;
}

}  // namespace detail

/// Describes one reflected field: PHX_FIELD(Rigidbody, mass).
#define PHX_FIELD(Type, member)                                                \
    ::phoenix::ecs::FieldInfo {                                                \
        #member, ::phoenix::ecs::detail::field_kind<decltype(Type::member)>(), \
            static_cast<std::uint32_t>(offsetof(Type, member)),                \
            static_cast<std::uint32_t>(sizeof(Type::member))                   \
    }

/// Process-wide registry. Ids are dense (0..count-1), assigned at registration. Register
/// components on the main thread before systems run (lookups are then lock-free reads).
class ComponentRegistry {
  public:
    static ComponentRegistry& instance();

    template <typename T>
    ComponentId register_component(const String& name, const String& category = "Scripts",
                                   LocalVector<FieldInfo> fields = {},
                                   ComponentStatus status = ComponentStatus::DataOnly,
                                   const String& unity_name = String(),
                                   const String& doc = String());

    /// Id of T, registering it with defaults on first use (name = C++ type name).
    template <typename T>
    ComponentId id_of();

    const ComponentInfo& info(ComponentId id) const { return *infos_[id]; }
    const ComponentInfo* find(const String& name) const;
    std::uint32_t count() const { return infos_.size(); }

  private:
    template <typename T>
    static ComponentId& slot() {
        static ComponentId id = kInvalidComponent;
        return id;
    }
    ComponentId add(ComponentInfo* info);

    LocalVector<ComponentInfo*> infos_;  // owned for the process lifetime
    // Plain Strings (not StringName): this registry outlives the foundation's StringName table.
    HashMap<String, ComponentId> by_name_;
};

template <typename T>
ComponentId ComponentRegistry::register_component(const String& name, const String& category,
                                                  LocalVector<FieldInfo> fields,
                                                  ComponentStatus status, const String& unity_name,
                                                  const String& doc) {
    static_assert(std::is_default_constructible_v<T>, "components must be default constructible");
    static_assert(std::is_move_constructible_v<T>, "components must be movable");
    ComponentId& id = slot<T>();
    ComponentInfo* info = nullptr;
    if (id != kInvalidComponent) {
        info = infos_[id];  // implicitly registered earlier: upgrade its metadata
        by_name_.erase(info->name);
    } else {
        info = new ComponentInfo();
        info->type = &typeid(T);
        if constexpr (std::is_empty_v<T>) {
            info->size = 0;
            info->align = 1;
        } else {
            info->size = sizeof(T);
            info->align = alignof(T);
        }
        info->construct = [](void* dst) { new (dst) T(); };
        info->destroy = [](void* p) { static_cast<T*>(p)->~T(); };
        info->move_construct = [](void* dst, void* src) {
            new (dst) T(std::move(*static_cast<T*>(src)));
        };
        if constexpr (std::is_copy_constructible_v<T>) {
            info->copy_construct = [](void* dst, const void* src) {
                new (dst) T(*static_cast<const T*>(src));
            };
        }
        info->name = name;
        id = add(info);
    }
    info->name = name;
    info->category = category;
    info->fields = std::move(fields);
    info->status = status;
    info->unity_name = unity_name.is_empty() ? name : unity_name;
    info->doc = doc;
    by_name_[info->name] = id;
    return id;
}

template <typename T>
ComponentId ComponentRegistry::id_of() {
    using U = std::remove_cvref_t<T>;
    const ComponentId id = slot<U>();
    return id != kInvalidComponent ? id : register_component<U>(String(typeid(U).name()));
}

template <typename T>
inline ComponentId component_id() {
    return ComponentRegistry::instance().id_of<T>();
}

}  // namespace phoenix::ecs
