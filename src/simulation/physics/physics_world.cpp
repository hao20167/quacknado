#include "physics_world.hpp"
#include "Jolt/Physics/EPhysicsUpdateError.h"

#include <SDL3/SDL.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>


constexpr JPH::ObjectLayer STATIC_LAYER = 0;
constexpr JPH::ObjectLayer DYNAMIC_LAYER = 1;

JPH::Vec3 ToVec3(simd_float3 v) {
  return {v.x, v.y, v.z};
}

JPH::RVec3 ToPosition(simd_float3 v) {
  return {v.x, v.y, v.z};
}

template<class T>
simd_float3 ToSimd(const T& val) {
  return {
    static_cast<float>(val.GetX()),
    static_cast<float>(val.GetY()),
    static_cast<float>(val.GetZ())
  };
}

struct PhysicsWorld::Impl {
  JPH::BroadPhaseLayerInterfaceTable broad_phase {2, 2};
  JPH::ObjectLayerPairFilterTable object_pairs {2};
  std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> broad_phase_filter;
  JPH::TempAllocatorImpl allocator {10 * 1024 * 1024};
  JPH::JobSystemSingleThreaded jobs {JPH::cMaxPhysicsJobs};
  JPH::PhysicsSystem system;
  std::vector<JPH::BodyID> bodies;

  Impl() {
    broad_phase.MapObjectToBroadPhaseLayer(STATIC_LAYER, JPH::BroadPhaseLayer(0));
    broad_phase.MapObjectToBroadPhaseLayer(DYNAMIC_LAYER, JPH::BroadPhaseLayer(1));

    object_pairs.EnableCollision(STATIC_LAYER, DYNAMIC_LAYER);
    object_pairs.EnableCollision(DYNAMIC_LAYER, DYNAMIC_LAYER);
    
    broad_phase_filter = std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(broad_phase, 2, object_pairs, 2);

    system.Init(1024, 0, 4096, 4096, broad_phase, *broad_phase_filter, object_pairs);

    system.SetGravity(JPH::Vec3(0, -9.81f, 0));
  }

  ~Impl() {
    auto& api = system.GetBodyInterface();
    for (JPH::BodyID id: bodies) {
      api.RemoveBody(id);
      api.DestroyBody(id);
    }
  }

  JPH::BodyID CreateBox(simd_float3 position, simd_float3 half_extent, bool dynamic, float mass) {
    JPH::ShapeRefC shape = new JPH::BoxShape(ToVec3(half_extent));
    JPH::BodyCreationSettings settings(
      shape,
      ToPosition(position),
      JPH::Quat::sIdentity(),
      dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
      dynamic ? DYNAMIC_LAYER : STATIC_LAYER
    );

    settings.mFriction = 0.5f;
    settings.mRestitution = 0.1f;

    if (dynamic) {
      settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
      settings.mMassPropertiesOverride.mMass = mass;
    }

    auto& api = system.GetBodyInterface();
    const auto id = api.CreateAndAddBody(settings, dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);

    if (id.IsInvalid()) {
      SDL_Log("Create jolt body failed.\n");
    }

    bodies.push_back(id);
    return id;
  }
};


PhysicsWorld::PhysicsWorld() {
  JPH::RegisterDefaultAllocator();
  JPH::Factory::sInstance = new JPH::Factory();
  JPH::RegisterTypes();
  impl_ = std::make_unique<Impl>();
}

PhysicsWorld::~PhysicsWorld() {
  impl_.reset();
  JPH::UnregisterTypes();
  delete JPH::Factory::sInstance;
  JPH::Factory::sInstance = nullptr;
}

JPH::BodyID PhysicsWorld::CreateStaticBox(simd_float3 position, simd_float3 half_extent) {
  return impl_->CreateBox(position, half_extent, false, 0);
}

JPH::BodyID PhysicsWorld::CreateDynamicBox(simd_float3 position, simd_float3 half_extent, float mass) {
  if (mass <= 0) {
    SDL_Log("CreateDynamicBox failed due to invlid mass value.\n");
    return {};
  }
  return impl_->CreateBox(position, half_extent, true, mass);
}

void PhysicsWorld::ResetBody(JPH::BodyID id, simd_float3 position) {
  auto& api = impl_->system.GetBodyInterface();
  api.SetPositionAndRotation(id, ToPosition(position), JPH::Quat::sIdentity(), JPH::EActivation::Activate);
  api.SetLinearAndAngularVelocity(id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
}

BodyState PhysicsWorld::ReadState(JPH::BodyID id) const {
  const auto& api = impl_->system.GetBodyInterface();

  JPH::RVec3 position;
  JPH::Quat rotation;
  api.GetPositionAndRotation(id, position, rotation);

  JPH::Vec3 linear_velocity;
  JPH::Vec3 angular_velogity;
  api.GetLinearAndAngularVelocity(id, linear_velocity, angular_velogity);

  return {
    ToSimd(position),
    ToSimd(api.GetCenterOfMassPosition(id)),
    simd_quaternion(rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW()),
    ToSimd(linear_velocity),
    ToSimd(angular_velogity)
  };
}

void PhysicsWorld::ApplyForceAt(JPH::BodyID id, simd_float3 force, simd_float3 world_point) {
  impl_->system.GetBodyInterface().AddForce(id, ToVec3(force), ToPosition(world_point));
}

void PhysicsWorld::Step(const float& dt) {
  const auto error = impl_->system.Update(dt, 1, &impl_->allocator, &impl_->jobs);
  if (error != JPH::EPhysicsUpdateError::None) {
    SDL_Log("Jolt physics update failed.\n");
  }
}
