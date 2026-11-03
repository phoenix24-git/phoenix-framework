// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "physics/physics_world.h"

#include <Jolt/Jolt.h>
// Jolt.h must come first.
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_timer.h>

#include <mutex>

#include "core/job_system.h"
#include "core/log.h"
#include "physics/jolt_job_system.h"

namespace phoenix {

namespace {

namespace Layers {
constexpr JPH::ObjectLayer kStatic = 0;
constexpr JPH::ObjectLayer kMoving = 1;
}  // namespace Layers

namespace BroadPhaseLayers {
constexpr JPH::BroadPhaseLayer kStatic(0);
constexpr JPH::BroadPhaseLayer kMoving(1);
constexpr JPH::uint kCount = 2;
}  // namespace BroadPhaseLayers

class BpLayerInterface final : public JPH::BroadPhaseLayerInterface {
  public:
    JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::kCount; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return layer == Layers::kStatic ? BroadPhaseLayers::kStatic : BroadPhaseLayers::kMoving;
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer == BroadPhaseLayers::kStatic ? "static" : "moving";
    }
#endif
};

class ObjectVsBpFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
  public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bp) const override {
        return layer == Layers::kMoving || bp == BroadPhaseLayers::kMoving;
    }
};

class ObjectPairFilter final : public JPH::ObjectLayerPairFilter {
  public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        return a == Layers::kMoving || b == Layers::kMoving;
    }
};

// Jolt's type registry is process-global; reference-count it so several
// worlds (e.g. tests) can coexist.
std::mutex g_jolt_mutex;
int g_jolt_refs = 0;

void acquire_jolt() {
    std::lock_guard lock(g_jolt_mutex);
    if (g_jolt_refs++ == 0) {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
}

void release_jolt() {
    std::lock_guard lock(g_jolt_mutex);
    if (--g_jolt_refs == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

}  // namespace

struct PhysicsWorld::Impl {
    struct PendingBox {
        std::uint32_t id;
        float x, y, half_w, half_h, angle;
        bool dynamic;
        std::uint32_t rgba;
    };
    struct PendingImpulse {
        std::uint32_t id;
        float ix, iy;
    };
    struct BodyInfo {
        JPH::BodyID body;
        float half_w, half_h;
        std::uint32_t rgba;
        bool dynamic;
    };

    BpLayerInterface bp_layers;
    ObjectVsBpFilter object_vs_bp;
    ObjectPairFilter object_pairs;
    std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator;
    std::unique_ptr<JoltJobSystem> job_adapter;
    JPH::PhysicsSystem system;
    std::vector<PendingBox> pending_boxes;
    std::vector<PendingImpulse> pending_impulses;
    std::vector<BodyInfo> bodies;  // index == public id
    std::uint32_t next_id = 0;
};

PhysicsWorld::PhysicsWorld() = default;
PhysicsWorld::~PhysicsWorld() {
    shutdown();
}

bool PhysicsWorld::init(JobSystem& jobs, std::uint32_t max_bodies) {
    acquire_jolt();
    jobs_ = &jobs;
    impl_ = std::make_unique<Impl>();
    command_mutex_ = SDL_CreateMutex();
    impl_->temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(16 * 1024 * 1024);
    impl_->job_adapter =
        std::make_unique<JoltJobSystem>(jobs, JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers);
    impl_->system.Init(max_bodies, /*num_body_mutexes=*/0, max_bodies, max_bodies, impl_->bp_layers,
                       impl_->object_vs_bp, impl_->object_pairs);
    impl_->system.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
    PHX_LOG_INFO("Physics: Jolt %d.%d.%d ready (max %u bodies, %d-way concurrency)",
                 JPH_VERSION_MAJOR, JPH_VERSION_MINOR, JPH_VERSION_PATCH, max_bodies,
                 impl_->job_adapter->GetMaxConcurrency());
    return command_mutex_ != nullptr;
}

void PhysicsWorld::shutdown() {
    if (!impl_) {
        return;
    }
    // Never tear down while a step is in flight on a worker.
    while (stepping_.load(std::memory_order_acquire) &&
           !step_ready_.load(std::memory_order_acquire)) {
        SDL_DelayNS(50'000);
    }
    stepping_.store(false);
    step_ready_.store(false);
    JPH::BodyInterface& bi = impl_->system.GetBodyInterface();
    for (const auto& info : impl_->bodies) {
        if (!info.body.IsInvalid()) {
            bi.RemoveBody(info.body);
            bi.DestroyBody(info.body);
        }
    }
    impl_.reset();
    if (command_mutex_) {
        SDL_DestroyMutex(command_mutex_);
        command_mutex_ = nullptr;
    }
    front_.clear();
    back_.clear();
    release_jolt();
}

std::uint32_t PhysicsWorld::add_box(float x, float y, float half_w, float half_h, float angle,
                                    bool dynamic, std::uint32_t rgba) {
    SDL_LockMutex(command_mutex_);
    const std::uint32_t id = impl_->next_id++;
    impl_->pending_boxes.push_back({id, x, y, half_w, half_h, angle, dynamic, rgba});
    SDL_UnlockMutex(command_mutex_);
    return id;
}

void PhysicsWorld::add_impulse(std::uint32_t id, float ix, float iy) {
    SDL_LockMutex(command_mutex_);
    impl_->pending_impulses.push_back({id, ix, iy});
    SDL_UnlockMutex(command_mutex_);
}

bool PhysicsWorld::begin_step(float dt) {
    if (stepping_.exchange(true, std::memory_order_acq_rel)) {
        return false;
    }
    pending_dt_ = dt;
    jobs_->submit(&PhysicsWorld::step_job, this);
    return true;
}

bool PhysicsWorld::poll_step() {
    if (!step_ready_.load(std::memory_order_acquire)) {
        return false;
    }
    front_.swap(back_);
    ++steps_;
    step_ready_.store(false, std::memory_order_relaxed);
    stepping_.store(false, std::memory_order_release);
    return true;
}

void PhysicsWorld::step(float dt) {
    while (!begin_step(dt)) {
        poll_step();
    }
    while (!poll_step()) {
        SDL_DelayNS(10'000);
    }
}

void PhysicsWorld::step_job(void* self) {
    static_cast<PhysicsWorld*>(self)->run_step();
}

void PhysicsWorld::run_step() {
    Impl& w = *impl_;
    JPH::BodyInterface& bi = w.system.GetBodyInterface();

    // 1) Apply commands queued by the main thread / scripts.
    std::vector<Impl::PendingBox> boxes;
    std::vector<Impl::PendingImpulse> impulses;
    SDL_LockMutex(command_mutex_);
    boxes.swap(w.pending_boxes);
    impulses.swap(w.pending_impulses);
    SDL_UnlockMutex(command_mutex_);

    for (const auto& b : boxes) {
        JPH::BodyCreationSettings settings(
            new JPH::BoxShape(JPH::Vec3(b.half_w, b.half_h, 0.5f)), JPH::RVec3(b.x, b.y, 0.0f),
            JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), b.angle),
            b.dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
            b.dynamic ? Layers::kMoving : Layers::kStatic);
        settings.mAllowedDOFs = JPH::EAllowedDOFs::Plane2D;
        settings.mRestitution = 0.25f;
        settings.mFriction = 0.6f;
        const JPH::BodyID body = bi.CreateAndAddBody(
            settings, b.dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
        if (w.bodies.size() <= b.id) {
            w.bodies.resize(b.id + 1, Impl::BodyInfo{JPH::BodyID(), 0, 0, 0, false});
        }
        w.bodies[b.id] = {body, b.half_w, b.half_h, b.rgba, b.dynamic};
    }
    for (const auto& imp : impulses) {
        if (imp.id < w.bodies.size() && !w.bodies[imp.id].body.IsInvalid()) {
            bi.AddImpulse(w.bodies[imp.id].body, JPH::Vec3(imp.ix, imp.iy, 0.0f));
        }
    }
    if (!boxes.empty()) {
        w.system.OptimizeBroadPhase();
    }

    // 2) Simulate (Jolt fans out onto the shared worker pool).
    if (pending_dt_ > 0.0f) {
        const int substeps = pending_dt_ > 1.0f / 50.0f ? 2 : 1;
        w.system.Update(pending_dt_, substeps, w.temp_allocator.get(), w.job_adapter.get());
    }

    // 3) Publish into the back buffer; main thread swaps in poll_step().
    back_.clear();
    back_.reserve(w.bodies.size());
    for (std::uint32_t id = 0; id < w.bodies.size(); ++id) {
        const auto& info = w.bodies[id];
        if (info.body.IsInvalid()) {
            continue;
        }
        JPH::RVec3 pos;
        JPH::Quat rot;
        bi.GetPositionAndRotation(info.body, pos, rot);
        JPH::Vec3 axis;
        float angle = 0.0f;
        rot.GetAxisAngle(axis, angle);
        if (axis.GetZ() < 0.0f) {
            angle = -angle;
        }
        back_.push_back(PhxBody2D{static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()),
                                  angle, info.half_w, info.half_h, id, info.rgba,
                                  info.dynamic ? 1u : 0u});
    }
    step_ready_.store(true, std::memory_order_release);
}

}  // namespace phoenix
