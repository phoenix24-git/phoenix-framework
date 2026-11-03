// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/scene/scene.h"

#include <algorithm>
#include <unordered_map>

#include "core/math/math_funcs.h"

namespace phoenix::scene {

namespace {

Transform3D trs(const LocalTransform& t) {
    return Transform3D(Basis(t.rotation, t.scale), t.position);
}

Vector3 deg_to_rad(const Vector3& d) {
    return Vector3(Math::deg_to_rad(d.x), Math::deg_to_rad(d.y), Math::deg_to_rad(d.z));
}
Vector3 rad_to_deg(const Vector3& r) {
    return Vector3(Math::rad_to_deg(r.x), Math::rad_to_deg(r.y), Math::rad_to_deg(r.z));
}
/// Unity applies Euler rotations Z, then X, then Y: the foundation's YXZ order.
Quaternion euler_to_quat(const Vector3& degrees) {
    return Quaternion::from_euler(deg_to_rad(degrees));
}
Vector3 quat_to_euler(const Quaternion& q) {
    return rad_to_deg(q.get_euler());
}

void register_scene_components() {
    static bool done = false;
    if (done)
        return;
    done = true;
    register_builtin_components();
    ecs::ComponentRegistry::instance().register_component<ScriptHost>(
        "ScriptHost", "Scripts", {}, ecs::ComponentStatus::Simulated,
        "MonoBehaviour (Script) -> MetaBehaviour", "Holds the entity's MetaBehaviour instances.");
}

}  // namespace

// --- Transform --------------------------------------------------------------------------------

bool Transform::valid() const {
    return scene_ && scene_->world().alive(entity_);
}
GameObject Transform::gameObject() const {
    return GameObject(scene_, entity_);
}

LocalTransform* Transform::local() const {
    return scene_ ? scene_->world().get<LocalTransform>(entity_) : nullptr;
}

Vector3 Transform::localPosition() const {
    return local() ? local()->position : Vector3();
}
void Transform::SetLocalPosition(const Vector3& p) {
    if (auto* l = local())
        l->position = p;
}
Quaternion Transform::localRotation() const {
    return local() ? local()->rotation : Quaternion();
}
void Transform::SetLocalRotation(const Quaternion& q) {
    if (auto* l = local())
        l->rotation = q.normalized();
}
Vector3 Transform::localScale() const {
    return local() ? local()->scale : Vector3(1, 1, 1);
}
void Transform::SetLocalScale(const Vector3& s) {
    if (auto* l = local())
        l->scale = s;
}
Vector3 Transform::localEulerAngles() const {
    return quat_to_euler(localRotation());
}
void Transform::SetLocalEulerAngles(const Vector3& d) {
    SetLocalRotation(euler_to_quat(d));
}

Transform3D Transform::localToWorldMatrix() const {
    Transform3D m;
    for (Transform t = *this; t.valid(); t = t.parent()) {
        if (const LocalTransform* l = t.local())
            m = trs(*l) * m;
    }
    return m;
}

Vector3 Transform::position() const {
    return localToWorldMatrix().origin;
}

void Transform::SetPosition(const Vector3& p) {
    const Transform par = parent();
    SetLocalPosition(par.valid() ? par.localToWorldMatrix().affine_inverse().xform(p) : p);
}

Quaternion Transform::rotation() const {
    Quaternion q = localRotation();
    for (Transform t = parent(); t.valid(); t = t.parent())
        q = t.localRotation() * q;
    return q.normalized();
}

void Transform::SetRotation(const Quaternion& q) {
    const Transform par = parent();
    SetLocalRotation(par.valid() ? par.rotation().inverse() * q : q);
}

Vector3 Transform::eulerAngles() const {
    return quat_to_euler(rotation());
}
void Transform::SetEulerAngles(const Vector3& d) {
    SetRotation(euler_to_quat(d));
}
Vector3 Transform::lossyScale() const {
    return localToWorldMatrix().basis.get_scale();
}
Vector3 Transform::forward() const {
    return rotation().xform(Vector3(0, 0, -1));
}
Vector3 Transform::right() const {
    return rotation().xform(Vector3(1, 0, 0));
}
Vector3 Transform::up() const {
    return rotation().xform(Vector3(0, 1, 0));
}

void Transform::Translate(const Vector3& delta, bool world_space) {
    if (world_space) {
        SetPosition(position() + delta);
    } else {
        SetLocalPosition(localPosition() + localRotation().xform(delta));
    }
}

void Transform::Rotate(const Vector3& euler_degrees, bool world_space) {
    const Quaternion r = euler_to_quat(euler_degrees);
    if (world_space) {
        SetRotation(r * rotation());
    } else {
        SetLocalRotation(localRotation() * r);
    }
}

void Transform::RotateAround(const Vector3& point, const Vector3& axis, float degrees) {
    const Quaternion r(axis.normalized(), Math::deg_to_rad(degrees));
    SetPosition(point + r.xform(position() - point));
    SetRotation(r * rotation());
}

void Transform::LookAt(const Vector3& target, const Vector3& world_up) {
    const Vector3 dir = target - position();
    if (dir.length_squared() < 1e-12f)
        return;
    // Basis::looking_at points -Z at the target: Phoenix's forward.
    SetRotation(Basis::looking_at(dir, world_up).get_rotation_quaternion());
}

Vector3 Transform::TransformPoint(const Vector3& p) const {
    return localToWorldMatrix().xform(p);
}
Vector3 Transform::InverseTransformPoint(const Vector3& p) const {
    return localToWorldMatrix().affine_inverse().xform(p);
}
Vector3 Transform::TransformDirection(const Vector3& d) const {
    return rotation().xform(d);
}

Transform Transform::parent() const {
    if (!scene_)
        return {};
    const Parent* p = scene_->world().get<Parent>(entity_);
    return p ? Transform(scene_, p->value) : Transform();
}

void Transform::SetParent(Transform new_parent, bool world_position_stays) {
    if (!valid() || new_parent == *this || (new_parent.valid() && new_parent.IsChildOf(*this)))
        return;
    ecs::World& w = scene_->world();
    const Transform3D world = localToWorldMatrix();
    if (Transform old = parent(); old.valid()) {
        if (Children* c = w.get<Children>(old.entity_))
            c->items.erase(entity_);
    }
    if (new_parent.valid()) {
        w.add<Parent>(entity_, Parent{new_parent.entity_});
        Children* c = w.get<Children>(new_parent.entity_);
        if (!c)
            c = &w.add<Children>(new_parent.entity_);
        c->items.push_back(entity_);
    } else {
        w.remove<Parent>(entity_);
    }
    if (world_position_stays) {
        const Transform3D local_m =
            new_parent.valid() ? new_parent.localToWorldMatrix().affine_inverse() * world : world;
        if (LocalTransform* l = local()) {
            l->position = local_m.origin;
            l->rotation = local_m.basis.get_rotation_quaternion();
            l->scale = local_m.basis.get_scale();
        }
    }
}

int Transform::childCount() const {
    const Children* c = scene_ ? scene_->world().get<Children>(entity_) : nullptr;
    return c ? int(c->items.size()) : 0;
}

Transform Transform::GetChild(int index) const {
    const Children* c = scene_ ? scene_->world().get<Children>(entity_) : nullptr;
    return (c && index >= 0 && index < c->items.size()) ? Transform(scene_, c->items[index])
                                                        : Transform();
}

Transform Transform::Find(const String& name) const {
    // Unity semantics: direct children only; "a/b/c" walks a path.
    const int slash = name.find("/");
    const String head = slash >= 0 ? name.substr(0, slash) : name;
    for (int i = 0; i < childCount(); ++i) {
        const Transform child = GetChild(i);
        if (child.gameObject().name() == head) {
            return slash >= 0 ? child.Find(name.substr(slash + 1)) : child;
        }
    }
    return {};
}

Transform Transform::root() const {
    Transform t = *this;
    while (t.parent().valid())
        t = t.parent();
    return t;
}

bool Transform::IsChildOf(Transform p) const {
    for (Transform t = *this; t.valid(); t = t.parent()) {
        if (t == p)
            return true;
    }
    return false;
}

int Transform::GetSiblingIndex() const {
    const Transform p = parent();
    const Children* c = p.valid() ? scene_->world().get<Children>(p.entity_) : nullptr;
    return c ? int(c->items.find(entity_)) : 0;
}

// --- GameObject -------------------------------------------------------------------------------

bool GameObject::valid() const {
    return scene_ && scene_->world().alive(entity_);
}

String GameObject::name() const {
    const Name* n = valid() ? scene_->world().get<Name>(entity_) : nullptr;
    return n ? n->value : String();
}
void GameObject::SetName(const String& name) {
    if (valid())
        scene_->world().add<Name>(entity_, Name{name});
}
String GameObject::tag() const {
    const Tag* t = valid() ? scene_->world().get<Tag>(entity_) : nullptr;
    return t ? t->value : String("Untagged");
}
void GameObject::SetTag(const String& tag) {
    if (valid())
        scene_->world().add<Tag>(entity_, Tag{tag});
}
int GameObject::layer() const {
    const Layer* l = valid() ? scene_->world().get<Layer>(entity_) : nullptr;
    return l ? l->value : 0;
}
void GameObject::SetLayer(int layer) {
    if (valid())
        scene_->world().add<Layer>(entity_, Layer{layer});
}

bool GameObject::activeSelf() const {
    return valid() && !scene_->world().has<Inactive>(entity_);
}

bool GameObject::activeInHierarchy() const {
    for (Transform t = transform(); t.valid(); t = t.parent()) {
        if (scene_->world().has<Inactive>(t.entity()))
            return false;
    }
    return valid();
}

void GameObject::SetActive(bool active) {
    if (!valid() || activeSelf() == active)
        return;
    const bool was_active = activeInHierarchy();
    if (active) {
        scene_->world().remove<Inactive>(entity_);
    } else {
        scene_->world().add<Inactive>(entity_);
    }
    if (activeInHierarchy() != was_active)
        scene_->NotifyActiveChanged(*this, active);
}

// --- MetaBehaviour ----------------------------------------------------------------------------

void MetaBehaviour::SetEnabled(bool enabled) {
    if (enabled_ == enabled)
        return;
    enabled_ = enabled;
    if (!awoken_ || destroyed_ || !game_object_.activeInHierarchy())
        return;
    if (enabled) {
        enable_called_ = true;
        OnEnable();
    } else if (enable_called_) {
        enable_called_ = false;
        OnDisable();
    }
}

// --- Scene ------------------------------------------------------------------------------------

Scene::Scene() {
    register_scene_components();
    // Transform hierarchy -> LocalToWorld (PostUpdate), so renderers/physics read world matrices.
    scheduler_.add<ecs::Read<LocalTransform>, ecs::Read<Parent>, ecs::Write<LocalToWorld>>(
        "TransformSystem", ecs::Stage::PostUpdate, [](ecs::SystemContext& ctx) {
            ecs::World& w = ctx.world;
            w.query<const LocalTransform, LocalToWorld>().without<Parent>().each(
                [](const LocalTransform& l, LocalToWorld& out) { out.value = trs(l); });
            // Children in depth order so parents are always resolved first.
            struct Item {
                int depth;
                Entity e;
            };
            std::vector<Item> items;
            std::unordered_map<std::uint32_t, int> depth_of;
            auto depth = [&](Entity e, auto&& self) -> int {
                if (auto it = depth_of.find(e.index); it != depth_of.end())
                    return it->second;
                const Parent* p = w.get<Parent>(e);
                const int d = (p && w.alive(p->value)) ? self(p->value, self) + 1 : 0;
                depth_of[e.index] = d;
                return d;
            };
            w.query<const Parent>().with<LocalTransform, LocalToWorld>().each(
                [&](Entity e, const Parent&) { items.push_back({0, e}); });
            for (Item& it : items)
                it.depth = depth(it.e, depth);
            std::stable_sort(items.begin(), items.end(),
                             [](const Item& a, const Item& b) { return a.depth < b.depth; });
            for (const Item& it : items) {
                const Parent* p = w.get<Parent>(it.e);
                const LocalToWorld* parent_m = w.get<LocalToWorld>(p->value);
                const LocalTransform* l = w.get<LocalTransform>(it.e);
                w.get<LocalToWorld>(it.e)->value =
                    (parent_m ? parent_m->value : Transform3D()) * trs(*l);
            }
        });
}

Scene::~Scene() {
    // Unity calls OnDisable/OnDestroy for every live behaviour when a scene unloads.
    for (MetaBehaviour* b : behaviours_) {
        if (b->enable_called_)
            b->OnDisable();
        if (b->awoken_)
            b->OnDestroy();
        b->destroyed_ = true;
    }
    behaviours_.clear();
}

GameObject Scene::CreateGameObject(const String& name) {
    const Entity e = world_.create();
    world_.add<Name>(e, Name{name});
    world_.add<LocalTransform>(e);
    world_.add<LocalToWorld>(e);
    return GameObject(this, e);
}

void Scene::AttachBehaviour(GameObject go, std::unique_ptr<MetaBehaviour> behaviour) {
    MetaBehaviour* b = behaviour.get();
    b->game_object_ = go;
    ScriptHost* host = world_.get<ScriptHost>(go.entity());
    if (!host)
        host = &world_.add<ScriptHost>(go.entity());
    host->behaviours.push_back(std::move(behaviour));
    behaviours_.push_back(b);
    if (go.activeInHierarchy()) {
        b->awoken_ = true;
        b->Awake();
        if (b->enabled_ && !b->destroyed_) {
            b->enable_called_ = true;
            b->OnEnable();
        }
    }
}

void Scene::DestroyBehaviour(MetaBehaviour* b) {
    if (!b || b->destroyed_)
        return;
    if (b->enable_called_) {
        b->enable_called_ = false;
        b->OnDisable();
    }
    if (b->awoken_)
        b->OnDestroy();
    b->destroyed_ = true;
    behaviours_.erase(std::remove(behaviours_.begin(), behaviours_.end(), b), behaviours_.end());
    if (ScriptHost* host = world_.get<ScriptHost>(b->game_object_.entity())) {
        auto& list = host->behaviours;
        list.erase(
            std::remove_if(list.begin(), list.end(), [b](const auto& p) { return p.get() == b; }),
            list.end());
    }
}

void Scene::NotifyActiveChanged(GameObject go, bool active) {
    std::vector<Entity> hierarchy;
    CollectHierarchy(go.entity(), hierarchy);
    for (Entity e : hierarchy) {
        ScriptHost* host = world_.get<ScriptHost>(e);
        if (!host)
            continue;
        const bool now_active = GameObject(this, e).activeInHierarchy();
        for (auto& p : host->behaviours) {
            MetaBehaviour& b = *p;
            if (active && now_active) {
                if (!b.awoken_) {
                    b.awoken_ = true;
                    b.Awake();
                }
                if (b.enabled_ && !b.enable_called_) {
                    b.enable_called_ = true;
                    b.OnEnable();
                }
            } else if (!active && b.enable_called_) {
                b.enable_called_ = false;
                b.OnDisable();
            }
        }
    }
}

void Scene::CollectHierarchy(Entity e, std::vector<Entity>& out) {
    out.push_back(e);
    if (const Children* c = world_.get<Children>(e)) {
        const ::Vector<Entity> kids = c->items;  // copy: recursion may not mutate, but be safe
        for (const Entity& k : kids)
            CollectHierarchy(k, out);
    }
}

void Scene::Destroy(GameObject go) {
    if (!go.valid())
        return;
    if (std::find(pending_destroy_.begin(), pending_destroy_.end(), go.entity()) ==
        pending_destroy_.end()) {
        pending_destroy_.push_back(go.entity());
    }
    if (!in_update_)
        FlushDestroyed();
}

void Scene::DestroyImmediate(GameObject go) {
    if (go.valid())
        DestroyRecursive(go.entity());
}

void Scene::DestroyRecursive(Entity root) {
    if (!world_.alive(root))
        return;
    std::vector<Entity> hierarchy;
    CollectHierarchy(root, hierarchy);
    // Detach the root from its parent.
    GameObject(this, root).transform().SetParent(Transform(), false);
    for (Entity e : hierarchy) {
        if (ScriptHost* host = world_.get<ScriptHost>(e)) {
            std::vector<MetaBehaviour*> list;
            for (auto& p : host->behaviours)
                list.push_back(p.get());
            for (MetaBehaviour* b : list)
                DestroyBehaviour(b);
        }
    }
    for (auto it = hierarchy.rbegin(); it != hierarchy.rend(); ++it)
        world_.destroy(*it);
}

void Scene::FlushDestroyed() {
    std::vector<Entity> pending;
    pending.swap(pending_destroy_);
    for (Entity e : pending)
        DestroyRecursive(e);
}

GameObject Scene::Instantiate(GameObject original) {
    if (!original.valid())
        return {};
    const ecs::ComponentId skip[] = {ecs::component_id<Parent>(), ecs::component_id<Children>(),
                                     ecs::component_id<ScriptHost>()};
    const Entity copy = world_.create();
    const auto& reg = ecs::ComponentRegistry::instance();
    // Snapshot the id list: adding components below may not invalidate it (different entity).
    std::vector<ecs::ComponentId> ids(world_.components_of(original.entity()).begin(),
                                      world_.components_of(original.entity()).end());
    for (ecs::ComponentId id : ids) {
        if (std::find(std::begin(skip), std::end(skip), id) != std::end(skip))
            continue;
        const ecs::ComponentInfo& info = reg.info(id);
        void* dst = world_.add_component(copy, id);
        if (info.size != 0 && info.copy_construct) {
            info.destroy(dst);
            info.copy_construct(dst, world_.get_component(original.entity(), id, false));
        }
    }
    GameObject result(this, copy);
    const Transform src = original.transform();
    for (int i = 0; i < src.childCount(); ++i) {
        GameObject child = Instantiate(src.GetChild(i).gameObject());
        child.transform().SetParent(result.transform(), false);
    }
    return result;
}

GameObject Scene::Find(const String& name) {
    GameObject found;
    world_.query<const Name>().each([&](Entity e, const Name& n) {
        if (!found.valid() && n.value == name && GameObject(this, e).activeInHierarchy())
            found = GameObject(this, e);
    });
    return found;
}

std::vector<GameObject> Scene::FindGameObjectsWithTag(const String& tag) {
    std::vector<GameObject> out;
    world_.query<const Tag>().each([&](Entity e, const Tag& t) {
        if (t.value == tag)
            out.emplace_back(this, e);
    });
    return out;
}

void Scene::StartPending() {
    const std::size_t n = behaviours_.size();
    for (std::size_t i = 0; i < n && i < behaviours_.size(); ++i) {
        MetaBehaviour* b = behaviours_[i];
        if (!b->started_ && b->awoken_ && b->enabled_ && !b->destroyed_ &&
            b->game_object_.activeInHierarchy()) {
            b->started_ = true;
            b->Start();
        }
    }
}

void Scene::RunBehaviours(void (*call)(MetaBehaviour&)) {
    // Behaviours added during this pass first run next frame (after Start), like Unity.
    const std::size_t n = behaviours_.size();
    for (std::size_t i = 0; i < n && i < behaviours_.size(); ++i) {
        MetaBehaviour* b = behaviours_[i];
        if (b->started_ && b->enabled_ && !b->destroyed_ && b->game_object_.activeInHierarchy())
            call(*b);
    }
}

void Scene::Update(float delta_time) {
    in_update_ = true;
    Time::unscaledDeltaTime = delta_time;
    Time::deltaTime = delta_time * Time::timeScale;
    Time::time += Time::deltaTime;
    ++Time::frameCount;
    const std::uint64_t frame = Time::frameCount;

    StartPending();
    scheduler_.run_stage(world_, ecs::Stage::Initialization, Time::deltaTime, Time::time, frame,
                         parallel_);

    fixed_accumulator_ += Time::deltaTime;
    for (int steps = 0; fixed_accumulator_ >= Time::fixedDeltaTime && steps < 8; ++steps) {
        fixed_accumulator_ -= Time::fixedDeltaTime;
        RunBehaviours([](MetaBehaviour& b) { b.FixedUpdate(); });
        scheduler_.run_stage(world_, ecs::Stage::FixedUpdate, Time::fixedDeltaTime, Time::time,
                             frame, parallel_);
    }

    RunBehaviours([](MetaBehaviour& b) { b.Update(); });
    scheduler_.run_stage(world_, ecs::Stage::Update, Time::deltaTime, Time::time, frame, parallel_);
    RunBehaviours([](MetaBehaviour& b) { b.LateUpdate(); });
    scheduler_.run_stage(world_, ecs::Stage::LateUpdate, Time::deltaTime, Time::time, frame,
                         parallel_);
    scheduler_.run_stage(world_, ecs::Stage::PostUpdate, Time::deltaTime, Time::time, frame,
                         parallel_);
    scheduler_.run_stage(world_, ecs::Stage::PreRender, Time::deltaTime, Time::time, frame,
                         parallel_);

    in_update_ = false;
    FlushDestroyed();
}

}  // namespace phoenix::scene
