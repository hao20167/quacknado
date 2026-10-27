#pragma once 

#include <memory>

#include <simd/simd.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>

struct BodyState {
  simd_float3 position;
  simd_float3 center_off_mass;
  simd_quatf orientation;
  simd_float3 linear_velocity;
  simd_float3 angular_velogity;
};

class PhysicsWorld {
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  PhysicsWorld();
  ~PhysicsWorld();

  PhysicsWorld(const PhysicsWorld&) = delete;
  PhysicsWorld& operator=(const PhysicsWorld&) = delete;

  JPH::BodyID CreateStaticBox(simd_float3 position, simd_float3 half_extent);
  JPH::BodyID CreateDynamicBox(simd_float3 position, simd_float3 half_extent, float mass);

  void ResetBody(JPH::BodyID id, simd_float3 position);

  BodyState ReadState(JPH::BodyID id) const;

  void ApplyForceAt(JPH::BodyID id, simd_float3 force, simd_float3 world_point);

  void Step(const float& dt);
};
