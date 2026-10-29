#include "physics_world.hpp"
#include "Jolt/Physics/EPhysicsUpdateError.h"

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#if defined(__APPLE__)
#include <pthread.h>
#include <pthread/qos.h>
#endif

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <SDL3/SDL.h>

constexpr JPH::ObjectLayer STATIC_LAYER = 0;
constexpr JPH::ObjectLayer DYNAMIC_LAYER = 1;

JPH::Vec3 ToVec3(simd_float3 v) { return {v.x, v.y, v.z}; }

JPH::RVec3 ToPosition(simd_float3 v) { return {v.x, v.y, v.z}; }

template <class T> simd_float3 ToSimd(const T &val) {
  return {static_cast<float>(val.GetX()), static_cast<float>(val.GetY()),
          static_cast<float>(val.GetZ())};
}

struct PhysicsWorld::Impl {
  JPH::BroadPhaseLayerInterfaceTable broad_phase{2, 2};
  JPH::ObjectLayerPairFilterTable object_pairs{2};
  std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> broad_phase_filter;
  JPH::TempAllocatorImpl allocator{64 * 1024 * 1024};
  struct Listener final : JPH::ContactListener {
    std::atomic<unsigned> dynamic{0}, statics{0};
    void Count(const JPH::Body &a, const JPH::Body &b) {
      (a.IsDynamic() && b.IsDynamic() ? dynamic : statics).fetch_add(1, std::memory_order_relaxed);
    }
    void OnContactAdded(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &,
                        JPH::ContactSettings &) override {
      Count(a, b);
    }
    void OnContactPersisted(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &,
                            JPH::ContactSettings &) override {
      Count(a, b);
    }
  } listener;
  std::atomic<bool> worker_qos_failed{false};
  std::unique_ptr<JPH::JobSystem> jobs;
  JPH::PhysicsSystem system;
  std::vector<JPH::BodyID> bodies;

  explicit Impl(PhysicsOptions options) {
    if (options.workers < 0 || options.workers > 16)
      throw std::invalid_argument("Physics workers must be 0..16");
    if (options.worker_user_initiated && !options.workers)
      throw std::invalid_argument("Worker QoS requires background workers");
    if (options.workers) {
      auto pool = std::make_unique<JPH::JobSystemThreadPool>();
      if (options.worker_user_initiated) {
#if defined(__APPLE__)
        pool->SetThreadInitFunction([this](int) {
          if (pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0) != 0 ||
              qos_class_self() != QOS_CLASS_USER_INITIATED)
            worker_qos_failed.store(true, std::memory_order_relaxed);
        });
#else
        throw std::invalid_argument("Worker QoS experiment requires macOS");
#endif
      }
      pool->Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, options.workers);
      jobs = std::move(pool);
    } else {
      jobs = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
    }
    broad_phase.MapObjectToBroadPhaseLayer(STATIC_LAYER, JPH::BroadPhaseLayer(0));
    broad_phase.MapObjectToBroadPhaseLayer(DYNAMIC_LAYER, JPH::BroadPhaseLayer(1));

    object_pairs.EnableCollision(STATIC_LAYER, DYNAMIC_LAYER);
    if (options.dynamic_collisions)
      object_pairs.EnableCollision(DYNAMIC_LAYER, DYNAMIC_LAYER);

    broad_phase_filter =
        std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(broad_phase, 2, object_pairs, 2);

    system.Init(kPhysicsBodyCapacity, 0, 65536, 65536, broad_phase, *broad_phase_filter,
                object_pairs);

    if (options.contact_telemetry)
      system.SetContactListener(&listener);
    system.SetGravity(JPH::Vec3(0, -9.81f, 0));
  }

  ~Impl() {
    auto &api = system.GetBodyInterface();
    for (JPH::BodyID id : bodies) {
      api.RemoveBody(id);
      api.DestroyBody(id);
    }
  }

  JPH::BodyID CreateBody(simd_float3 position, const JPH::Shape *shape, bool dynamic, float mass,
                         simd_quatf orientation = simd_quaternion(0.0f, 0.0f, 0.0f, 1.0f)) {
    JPH::BodyCreationSettings settings(shape, ToPosition(position),
                                       JPH::Quat(orientation.vector.x, orientation.vector.y,
                                                 orientation.vector.z, orientation.vector.w),
                                       dynamic ? JPH::EMotionType::Dynamic
                                               : JPH::EMotionType::Static,
                                       dynamic ? DYNAMIC_LAYER : STATIC_LAYER);

    settings.mFriction = 0.5f;
    settings.mRestitution = 0.1f;

    if (dynamic) {
      settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
      settings.mMassPropertiesOverride.mMass = mass;
    }

    auto &api = system.GetBodyInterface();
    const auto id = api.CreateAndAddBody(settings, dynamic ? JPH::EActivation::Activate
                                                           : JPH::EActivation::DontActivate);

    if (id.IsInvalid()) {
      SDL_Log("Create jolt body failed.\n");
      return id;
    }

    bodies.push_back(id);
    return id;
  }

  JPH::BodyID CreateBox(simd_float3 position, simd_float3 half_extent, bool dynamic, float mass,
                        simd_quatf orientation = simd_quaternion(0.0f, 0.0f, 0.0f, 1.0f)) {
    JPH::ShapeRefC shape = new JPH::BoxShape(ToVec3(half_extent));
    return CreateBody(position, shape, dynamic, mass, orientation);
  }
};

PhysicsWorld::PhysicsWorld(PhysicsOptions options) {
  if (options.workers < 0 || options.workers > 16)
    throw std::invalid_argument("Physics workers must be 0..16");
  if (options.worker_user_initiated && !options.workers)
    throw std::invalid_argument("Worker QoS requires background workers");
#if !defined(__APPLE__)
  if (options.worker_user_initiated)
    throw std::invalid_argument("Worker QoS experiment requires macOS");
#endif
  JPH::RegisterDefaultAllocator();
  JPH::Factory::sInstance = new JPH::Factory();
  JPH::RegisterTypes();
  impl_ = std::make_unique<Impl>(options);
}

PhysicsWorld::~PhysicsWorld() {
  impl_.reset();
  JPH::UnregisterTypes();
  delete JPH::Factory::sInstance;
  JPH::Factory::sInstance = nullptr;
}

JPH::BodyID PhysicsWorld::CreateStaticBox(simd_float3 position, simd_float3 half_extent,
                                          simd_quatf orientation) {
  return impl_->CreateBox(position, half_extent, false, 0, orientation);
}

JPH::BodyID PhysicsWorld::CreateDynamicBox(simd_float3 position, simd_float3 half_extent,
                                           float mass) {
  if (mass <= 0) {
    SDL_Log("CreateDynamicBox failed due to invlid mass value.\n");
    return {};
  }
  return impl_->CreateBox(position, half_extent, true, mass);
}

JPH::BodyID PhysicsWorld::CreateDynamicCompound(simd_float3 position,
                                                std::span<const CollisionBox> boxes, float mass) {
  return CreateCompound(position, boxes, true, mass);
}

JPH::BodyID PhysicsWorld::CreateStaticCompound(simd_float3 position,
                                               std::span<const CollisionBox> boxes) {
  return CreateCompound(position, boxes, false, 0);
}

JPH::BodyID PhysicsWorld::CreateCompound(simd_float3 position, std::span<const CollisionBox> boxes,
                                         bool dynamic, float mass) {
  if (boxes.empty() || (dynamic && (!std::isfinite(mass) || mass <= 0))) {
    SDL_Log("Invalid compound boxes or mass.");
    return {};
  }
  JPH::StaticCompoundShapeSettings settings;
  for (const auto &part : boxes) {
    if (!simd_all(part.half_extent > simd_float3{0, 0, 0})) {
      SDL_Log("Compound box half extents must be positive.");
      return {};
    }
    JPH::ShapeRefC box = new JPH::BoxShape(ToVec3(part.half_extent));
    settings.AddShape(ToVec3(part.center), JPH::Quat::sIdentity(), box);
  }
  const auto shape = settings.Create();
  if (shape.HasError()) {
    SDL_Log("Compound shape failed: %s", shape.GetError().c_str());
    return {};
  }
  return impl_->CreateBody(position, shape.Get(), dynamic, mass);
}

bool PhysicsWorld::IsSpawnAreaFree(simd_float3 position, simd_float3 half_extent,
                                   JPH::BodyID ignore, float clearance) const {
  const auto extent = half_extent + std::max(clearance, 0.0f);
  const JPH::AABox query(ToVec3(position - extent), ToVec3(position + extent));
  struct Collector final : JPH::CollideShapeBodyCollector {
    const JPH::BodyLockInterface &locks;
    const JPH::AABox &query;
    JPH::BodyID ignore;
    bool occupied = false;
    Collector(const JPH::BodyLockInterface &body_locks, const JPH::AABox &box, JPH::BodyID id)
        : locks(body_locks), query(box), ignore(id) {}
    void AddHit(const JPH::BodyID &id) override {
      if (id == ignore)
        return;
      const JPH::BodyLockRead lock(locks, id);
      if (lock.Succeeded() && lock.GetBody().GetWorldSpaceBounds().Overlaps(query)) {
        occupied = true;
        ForceEarlyOut();
      }
    }
  } collector(impl_->system.GetBodyLockInterface(), query, ignore);
  impl_->system.GetBroadPhaseQuery().CollideAABox(query, collector);
  return !collector.occupied;
}

void PhysicsWorld::DeactivateBody(JPH::BodyID id) {
  impl_->system.GetBodyInterface().DeactivateBody(id);
}

void PhysicsWorld::ResetBody(JPH::BodyID id, simd_float3 position) {
  auto &api = impl_->system.GetBodyInterface();
  api.SetPositionAndRotation(id, ToPosition(position), JPH::Quat::sIdentity(),
                             JPH::EActivation::Activate);
  api.SetLinearAndAngularVelocity(id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
}

BodyState PhysicsWorld::ReadState(JPH::BodyID id) const {
  if (coalesced_state_reads) {
    const JPH::BodyLockRead lock(impl_->system.GetBodyLockInterface(), id);
    if (!lock.Succeeded())
      throw std::runtime_error("ReadState: invalid body ID");
    const auto &body = lock.GetBody();
    const auto q = body.GetRotation();
    return {ToSimd(body.GetPosition()), ToSimd(body.GetCenterOfMassPosition()),
            simd_quaternion(q.GetX(), q.GetY(), q.GetZ(), q.GetW()),
            ToSimd(body.GetLinearVelocity()), ToSimd(body.GetAngularVelocity())};
  }
  const auto &api = impl_->system.GetBodyInterface();

  JPH::RVec3 position;
  JPH::Quat rotation;
  api.GetPositionAndRotation(id, position, rotation);

  JPH::Vec3 linear_velocity;
  JPH::Vec3 angular_velogity;
  api.GetLinearAndAngularVelocity(id, linear_velocity, angular_velogity);

  return {ToSimd(position), ToSimd(api.GetCenterOfMassPosition(id)),
          simd_quaternion(rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW()),
          ToSimd(linear_velocity), ToSimd(angular_velogity)};
}

void PhysicsWorld::ApplyForceAt(JPH::BodyID id, simd_float3 force, simd_float3 world_point) {
  if (redundant_force_check &&
      impl_->system.GetBodyInterface().GetMotionType(id) == JPH::EMotionType::Static)
    return;
  impl_->system.GetBodyInterface().AddForce(id, ToVec3(force), ToPosition(world_point));
}

void PhysicsWorld::Step(const float &dt) {
  impl_->listener.dynamic = 0;
  impl_->listener.statics = 0;
  const auto error = impl_->system.Update(dt, 1, &impl_->allocator, impl_->jobs.get());
  if (impl_->worker_qos_failed.load(std::memory_order_relaxed))
    throw std::runtime_error("Failed to request/verify Jolt worker QoS");
  if (error != JPH::EPhysicsUpdateError::None) {
    throw std::runtime_error("Jolt physics update failed: " +
                             std::to_string(static_cast<unsigned>(error)));
  }
}

void PhysicsWorld::OptimizeBroadPhase() { impl_->system.OptimizeBroadPhase(); }

PhysicsWorld::Pose PhysicsWorld::ReadPose(JPH::BodyID id) const {
  JPH::RVec3 p;
  JPH::Quat q;
  impl_->system.GetBodyInterface().GetPositionAndRotation(id, p, q);
  return {ToSimd(p), simd_quaternion(q.GetX(), q.GetY(), q.GetZ(), q.GetW())};
}
unsigned PhysicsWorld::ActiveBodyCount() const {
  return impl_->system.GetNumActiveBodies(JPH::EBodyType::RigidBody);
}

ContactCounts PhysicsWorld::Contacts() const {
  return {impl_->listener.dynamic.load(), impl_->listener.statics.load()};
}
