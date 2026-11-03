// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/ecs/component.h"

#include <cstring>

namespace phoenix::ecs {

namespace {

template <typename T>
T& at(void* base, const FieldInfo& f) {
    return *reinterpret_cast<T*>(static_cast<std::byte*>(base) + f.offset);
}
template <typename T>
const T& at(const void* base, const FieldInfo& f) {
    return *reinterpret_cast<const T*>(static_cast<const std::byte*>(base) + f.offset);
}

std::int64_t entity_to_int(Entity e) {
    return static_cast<std::int64_t>(e.bits());
}
Entity int_to_entity(std::int64_t v) {
    const auto bits = static_cast<std::uint64_t>(v);
    return Entity{static_cast<std::uint32_t>(bits & 0xffffffffu),
                  static_cast<std::uint32_t>(bits >> 32)};
}

bool is_number(const Variant& v) {
    return v.get_type() == Variant::INT || v.get_type() == Variant::FLOAT;
}

}  // namespace

const FieldInfo* ComponentInfo::field(const char* field_name) const {
    for (const FieldInfo& f : fields) {
        if (std::strcmp(f.name, field_name) == 0)
            return &f;
    }
    return nullptr;
}

Variant ComponentInfo::get(const void* c, const FieldInfo& f) const {
    switch (f.kind) {
        case FieldKind::Bool:
            return at<bool>(c, f);
        case FieldKind::Int32:
            return at<std::int32_t>(c, f);
        case FieldKind::UInt32:
            return static_cast<std::int64_t>(at<std::uint32_t>(c, f));
        case FieldKind::Int64:
            return at<std::int64_t>(c, f);
        case FieldKind::UInt64:
            return static_cast<std::int64_t>(at<std::uint64_t>(c, f));
        case FieldKind::Float:
            return at<float>(c, f);
        case FieldKind::Double:
            return at<double>(c, f);
        case FieldKind::Vector2:
            return at<::Vector2>(c, f);
        case FieldKind::Vector3:
            return at<::Vector3>(c, f);
        case FieldKind::Vector4:
            return at<::Vector4>(c, f);
        case FieldKind::Quaternion:
            return at<::Quaternion>(c, f);
        case FieldKind::Color:
            return at<::Color>(c, f);
        case FieldKind::Transform3D:
            return at<::Transform3D>(c, f);
        case FieldKind::Entity:
            return entity_to_int(at<Entity>(c, f));
        case FieldKind::Asset:
            return at<AssetRef>(c, f).uid;
        case FieldKind::LayerMask:
            return static_cast<std::int64_t>(at<LayerMask>(c, f).value);
        case FieldKind::Enum:
            return at<std::int32_t>(c, f);
        case FieldKind::String:
            return at<::String>(c, f);
        case FieldKind::StringName:
            return at<::StringName>(c, f);
        case FieldKind::FloatArray: {
            PackedFloat32Array out;
            for (float v : at<::Vector<float>>(c, f))
                out.push_back(v);
            return out;
        }
        case FieldKind::Vector3Array: {
            PackedVector3Array out;
            for (const ::Vector3& v : at<::Vector<::Vector3>>(c, f))
                out.push_back(v);
            return out;
        }
        case FieldKind::AssetArray: {
            PackedInt64Array out;
            for (const AssetRef& a : at<::Vector<AssetRef>>(c, f))
                out.push_back(a.uid);
            return out;
        }
        case FieldKind::EntityArray: {
            PackedInt64Array out;
            for (const Entity& e : at<::Vector<Entity>>(c, f))
                out.push_back(entity_to_int(e));
            return out;
        }
    }
    return Variant();
}

bool ComponentInfo::set(void* c, const FieldInfo& f, const Variant& v) const {
    const Variant::Type t = v.get_type();
    switch (f.kind) {
        case FieldKind::Bool:
            if (t != Variant::BOOL)
                return false;
            at<bool>(c, f) = v;
            return true;
        case FieldKind::Int32:
        case FieldKind::Enum:
            if (!is_number(v))
                return false;
            at<std::int32_t>(c, f) = static_cast<std::int32_t>(static_cast<std::int64_t>(v));
            return true;
        case FieldKind::UInt32:
            if (!is_number(v))
                return false;
            at<std::uint32_t>(c, f) = static_cast<std::uint32_t>(static_cast<std::int64_t>(v));
            return true;
        case FieldKind::Int64:
            if (!is_number(v))
                return false;
            at<std::int64_t>(c, f) = v;
            return true;
        case FieldKind::UInt64:
            if (!is_number(v))
                return false;
            at<std::uint64_t>(c, f) = static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
            return true;
        case FieldKind::Float:
            if (!is_number(v))
                return false;
            at<float>(c, f) = static_cast<float>(static_cast<double>(v));
            return true;
        case FieldKind::Double:
            if (!is_number(v))
                return false;
            at<double>(c, f) = v;
            return true;
#define PHX_SET_TYPED(Kind, VType, CType) \
    case FieldKind::Kind:                 \
        if (t != Variant::VType)          \
            return false;                 \
        at<CType>(c, f) = v;              \
        return true;
            PHX_SET_TYPED(Vector2, VECTOR2, ::Vector2)
            PHX_SET_TYPED(Vector3, VECTOR3, ::Vector3)
            PHX_SET_TYPED(Vector4, VECTOR4, ::Vector4)
            PHX_SET_TYPED(Quaternion, QUATERNION, ::Quaternion)
            PHX_SET_TYPED(Color, COLOR, ::Color)
            PHX_SET_TYPED(Transform3D, TRANSFORM3D, ::Transform3D)
#undef PHX_SET_TYPED
        case FieldKind::String:
            if (t != Variant::STRING && t != Variant::STRING_NAME)
                return false;
            at<::String>(c, f) = v;
            return true;
        case FieldKind::StringName:
            if (t != Variant::STRING && t != Variant::STRING_NAME)
                return false;
            at<::StringName>(c, f) = v;
            return true;
        case FieldKind::Entity:
            if (t != Variant::INT)
                return false;
            at<Entity>(c, f) = int_to_entity(v);
            return true;
        case FieldKind::Asset:
            if (t != Variant::INT)
                return false;
            at<AssetRef>(c, f).uid = v;
            return true;
        case FieldKind::LayerMask:
            if (t != Variant::INT)
                return false;
            at<LayerMask>(c, f).value = static_cast<std::uint32_t>(static_cast<std::int64_t>(v));
            return true;
        case FieldKind::FloatArray: {
            if (t != Variant::PACKED_FLOAT32_ARRAY && t != Variant::PACKED_FLOAT64_ARRAY &&
                t != Variant::ARRAY)
                return false;
            const PackedFloat32Array in = v;
            ::Vector<float>& out = at<::Vector<float>>(c, f);
            out.clear();
            for (float x : in)
                out.push_back(x);
            return true;
        }
        case FieldKind::Vector3Array: {
            if (t != Variant::PACKED_VECTOR3_ARRAY && t != Variant::ARRAY)
                return false;
            at<::Vector<::Vector3>>(c, f) = PackedVector3Array(v);
            return true;
        }
        case FieldKind::AssetArray:
        case FieldKind::EntityArray: {
            if (t != Variant::PACKED_INT64_ARRAY && t != Variant::PACKED_INT32_ARRAY &&
                t != Variant::ARRAY)
                return false;
            const PackedInt64Array in = v;
            if (f.kind == FieldKind::AssetArray) {
                ::Vector<AssetRef>& out = at<::Vector<AssetRef>>(c, f);
                out.clear();
                for (std::int64_t x : in)
                    out.push_back(AssetRef{x});
            } else {
                ::Vector<Entity>& out = at<::Vector<Entity>>(c, f);
                out.clear();
                for (std::int64_t x : in)
                    out.push_back(int_to_entity(x));
            }
            return true;
        }
    }
    return false;
}

Dictionary ComponentInfo::to_dictionary(const void* c) const {
    Dictionary d;
    for (const FieldInfo& f : fields)
        d[String(f.name)] = get(c, f);
    return d;
}

int ComponentInfo::from_dictionary(void* c, const Dictionary& d) const {
    int applied = 0;
    for (const FieldInfo& f : fields) {
        const String key(f.name);
        if (d.has(key) && set(c, f, d[key]))
            ++applied;
    }
    return applied;
}

// --- Registry ---------------------------------------------------------------------------------

ComponentRegistry& ComponentRegistry::instance() {
    // Intentionally never destroyed: component ids are cached in function-local statics and
    // must stay valid for the whole process (including static destructors).
    static ComponentRegistry* registry = new ComponentRegistry();
    return *registry;
}

ComponentId ComponentRegistry::add(ComponentInfo* info) {
    const ComponentId id = infos_.size();
    info->id = id;
    infos_.push_back(info);
    return id;
}

const ComponentInfo* ComponentRegistry::find(const String& name) const {
    const ComponentId* id = by_name_.getptr(name);
    return id ? infos_[*id] : nullptr;
}

const char* field_kind_name(FieldKind kind) {
    switch (kind) {
        case FieldKind::Bool:
            return "bool";
        case FieldKind::Int32:
            return "int";
        case FieldKind::UInt32:
            return "uint";
        case FieldKind::Int64:
            return "long";
        case FieldKind::UInt64:
            return "ulong";
        case FieldKind::Float:
            return "float";
        case FieldKind::Double:
            return "double";
        case FieldKind::Vector2:
            return "Vector2";
        case FieldKind::Vector3:
            return "Vector3";
        case FieldKind::Vector4:
            return "Vector4";
        case FieldKind::Quaternion:
            return "Quaternion";
        case FieldKind::Color:
            return "Color";
        case FieldKind::Transform3D:
            return "Transform3D";
        case FieldKind::Entity:
            return "Entity";
        case FieldKind::Asset:
            return "Asset";
        case FieldKind::LayerMask:
            return "LayerMask";
        case FieldKind::Enum:
            return "enum";
        case FieldKind::String:
            return "String";
        case FieldKind::StringName:
            return "StringName";
        case FieldKind::FloatArray:
            return "float[]";
        case FieldKind::Vector3Array:
            return "Vector3[]";
        case FieldKind::AssetArray:
            return "Asset[]";
        case FieldKind::EntityArray:
            return "Entity[]";
    }
    return "?";
}

}  // namespace phoenix::ecs
