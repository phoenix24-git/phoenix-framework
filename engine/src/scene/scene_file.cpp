// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/scene/scene_file.h"

#include "core/io/file_access.h"
#include "core/variant/variant_parser.h"

namespace phoenix::scene {

namespace {

enum class Section { None, Node, Component, Script };

class Loader {
  public:
    Loader(Scene& scene, String source) : scene_(scene), source_(std::move(source)) {}

    SceneLoadResult run(const String& text) {
        VariantParser::StreamString stream;
        stream.s = text;
        int line = 1;
        while (true) {
            VariantParser::Tag tag;
            String assign;
            Variant value;
            String parse_error;
            const Error err = VariantParser::parse_tag_assign_eof(&stream, line, parse_error, tag,
                                                                  assign, value, nullptr, false);
            if (err == ERR_FILE_EOF) {
                break;
            }
            line_ = line;
            if (err != OK) {
                return fail(ERR_PARSE_ERROR, parse_error);
            }
            const bool ok = assign.is_empty() ? on_tag(tag) : on_assign(assign, value);
            if (!ok) {
                return fail(error_, message_);
            }
        }
        if (!header_) {
            line_ = 1;
            return fail(ERR_FILE_UNRECOGNIZED, "missing [phoenix_scene format=1] header");
        }
        if (!result_.root.valid()) {
            return fail(ERR_FILE_CORRUPT, "the scene has no [node]");
        }
        return result_;
    }

  private:
    bool error(Error err, const String& message) {
        error_ = err;
        message_ = message;
        return false;
    }

    SceneLoadResult fail(Error err, const String& message) {
        for (int64_t i = created_.size() - 1; i >= 0; --i) {
            if (created_[i].valid()) {
                scene_.DestroyImmediate(created_[i]);
            }
        }
        SceneLoadResult r;
        r.error = err;
        r.message = vformat("%s:%d: %s", source_, line_, message);
        return r;
    }

    bool on_tag(const VariantParser::Tag& tag) {
        if (!header_) {
            if (tag.name != "phoenix_scene") {
                return error(
                    ERR_FILE_UNRECOGNIZED,
                    "missing the [phoenix_scene format=1] header (found [" + tag.name + "])");
            }
            const Variant* format = tag.fields.getptr("format");
            if (!format || int(*format) != 1) {
                return error(ERR_FILE_UNRECOGNIZED, "unsupported scene format (expected format=1)");
            }
            header_ = true;
            return true;
        }
        if (tag.name == "node") {
            return on_node(tag);
        }
        if (tag.name == "component") {
            return on_component(tag);
        }
        if (tag.name == "script") {
            return on_script(tag);
        }
        return error(ERR_PARSE_ERROR, "unknown section [" + tag.name + "]");
    }

    bool on_node(const VariantParser::Tag& tag) {
        const Variant* name = tag.fields.getptr("name");
        if (!name || String(*name).is_empty()) {
            return error(ERR_PARSE_ERROR, "[node] needs a name");
        }
        const Variant* parent_path = tag.fields.getptr("parent");
        GameObject parent;
        if (result_.root.valid()) {
            if (!parent_path) {
                return error(ERR_PARSE_ERROR, "only the first [node] (the root) may omit parent=");
            }
            const String path = *parent_path;
            if (path == ".") {
                parent = result_.root;
            } else {
                const Transform found = result_.root.transform().Find(path);
                if (!found.valid()) {
                    return error(ERR_DOES_NOT_EXIST,
                                 "parent \"" + path + "\" not found under the root");
                }
                parent = found.gameObject();
            }
        } else if (parent_path) {
            return error(ERR_PARSE_ERROR, "the root [node] can't have a parent");
        }
        node_ = scene_.CreateGameObject(*name);
        created_.push_back(node_);
        if (parent.valid()) {
            node_.transform().SetParent(parent.transform(), /*world_position_stays=*/false);
        } else {
            result_.root = node_;
        }
        section_ = Section::Node;
        return true;
    }

    bool on_component(const VariantParser::Tag& tag) {
        if (!node_.valid()) {
            return error(ERR_PARSE_ERROR, "[component] before any [node]");
        }
        const Variant* type = tag.fields.getptr("type");
        if (!type) {
            return error(ERR_PARSE_ERROR, "[component] needs type=");
        }
        String name = *type;
        if (name == "Transform") {
            name = "LocalTransform";  // Unity's name for it
        }
        component_ = ecs::ComponentRegistry::instance().find(name);
        if (!component_) {
            return error(ERR_DOES_NOT_EXIST, "unknown component type \"" + String(*type) + "\"");
        }
        scene_.world().add_component(node_.entity(), component_->id);
        section_ = Section::Component;
        return true;
    }

    bool on_script(const VariantParser::Tag& tag) {
        if (!node_.valid()) {
            return error(ERR_PARSE_ERROR, "[script] before any [node]");
        }
        const Variant* cls = tag.fields.getptr("class");
        if (!cls || String(*cls).is_empty()) {
            return error(ERR_PARSE_ERROR, "[script] needs class=\"Namespace.ClassName\"");
        }
        SceneScript script;
        script.gameObject = node_;
        script.class_name = *cls;
        result_.scripts.push_back(script);
        section_ = Section::Script;
        return true;
    }

    bool on_assign(const String& key, const Variant& value) {
        switch (section_) {
            case Section::None:
                return error(ERR_PARSE_ERROR, "\"" + key + "\" outside of a section");
            case Section::Node:
                return assign_node(key, value);
            case Section::Component:
                return assign_component(key, value);
            case Section::Script:
                result_.scripts.write[result_.scripts.size() - 1].properties[key] = value;
                return true;
        }
        return false;
    }

    bool assign_node(const String& key, const Variant& value) {
        if (key == "tag" && value.get_type() == Variant::STRING) {
            node_.SetTag(value);
        } else if (key == "layer" && value.get_type() == Variant::INT) {
            node_.SetLayer(int(value));
        } else if (key == "active" && value.get_type() == Variant::BOOL) {
            node_.SetActive(bool(value));
        } else {
            return error(
                ERR_INVALID_PARAMETER,
                "node keys are tag (String), layer (int) and active (bool); got \"" + key + "\"");
        }
        return true;
    }

    bool assign_component(const String& key, const Variant& value) {
        if (component_->name == "LocalTransform" && key == "euler") {
            if (value.get_type() != Variant::VECTOR3) {
                return error(ERR_INVALID_PARAMETER, "euler expects Vector3(x, y, z) in degrees");
            }
            node_.transform().SetLocalEulerAngles(value);
            return true;
        }
        const ecs::FieldInfo* field = component_->field(key.utf8().get_data());
        if (!field) {
            return error(ERR_DOES_NOT_EXIST,
                         vformat("%s has no field \"%s\"", component_->name, key));
        }
        Variant v = value;
        if (field->kind == ecs::FieldKind::Enum && value.get_type() == Variant::STRING &&
            field->enum_names) {
            // Enums by name: projection = "Orthographic".
            v = Variant();
            for (int i = 0; field->enum_names[i]; ++i) {
                if (String(field->enum_names[i]) == String(value)) {
                    v = i;
                    break;
                }
            }
            if (v.get_type() == Variant::NIL) {
                return error(ERR_INVALID_PARAMETER, vformat("%s.%s has no value \"%s\"",
                                                            component_->name, key, String(value)));
            }
        }
        // Re-fetch every time: adding a component moves the entity to another chunk.
        void* storage = scene_.world().get_component(node_.entity(), component_->id, true);
        if (!storage || !component_->set(storage, *field, v)) {
            return error(ERR_INVALID_PARAMETER,
                         vformat("%s.%s: cannot assign a %s", component_->name, key,
                                 Variant::get_type_name(value.get_type())));
        }
        return true;
    }

    Scene& scene_;
    String source_;
    SceneLoadResult result_;
    Vector<GameObject> created_;
    GameObject node_;
    Section section_ = Section::None;
    const ecs::ComponentInfo* component_ = nullptr;
    bool header_ = false;
    int line_ = 0;
    Error error_ = OK;
    String message_;
};

}  // namespace

SceneLoadResult load_scene_from_string(Scene& scene, const String& text,
                                       const String& source_name) {
    return Loader(scene, source_name).run(text);
}

SceneLoadResult load_scene(Scene& scene, const String& path) {
    Error err = OK;
    const String text = FileAccess::get_file_as_string(path, &err);
    if (err != OK) {
        SceneLoadResult r;
        r.error = err;
        r.message = vformat("%s: cannot open the scene file", path);
        return r;
    }
    return load_scene_from_string(scene, text, path);
}

}  // namespace phoenix::scene
