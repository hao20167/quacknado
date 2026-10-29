#pragma once

#include <memory>
#include <span>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <simd/simd.h>

inline constexpr size_t kPhysicsBodyCapacity = 16384;
inline constexpr size_t kScenePropCount = 3;
inline constexpr size_t kMaxSceneObjects = kPhysicsBodyCapacity - kScenePropCount;

struct CollisionBox {
  simd_float3 center;
  simd_float3 half_extent;
};

struct BodyState {
  simd_float3 position;
  simd_float3 center_off_mass;
  simd_quatf orientation;
  simd_float3 linear_velocity;
  simd_float3 angular_velogity;
};

struct PhysicsOptions {
  int workers = 0;
  bool dynamic_collisions = true;
  bool contact_telemetry = false;
  bool worker_user_initiated = false; // macOS worker QoS experiment; never changes solver settings.
};
struct ContactCounts {
  unsigned dynamic = 0, static_contacts = 0;
};

class PhysicsWorld {
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;

public:
  bool coalesced_state_reads = true;
  bool redundant_force_check = false;
  explicit PhysicsWorld(PhysicsOptions options = {});
  ContactCounts Contacts() const;
  ~PhysicsWorld();

  PhysicsWorld(const PhysicsWorld &) = delete;
  PhysicsWorld &operator=(const PhysicsWorld &) = delete;

  JPH::BodyID CreateStaticBox(simd_float3 position, simd_float3 half_extent,
                              simd_quatf orientation = simd_quaternion(0.0f, 0.0f, 0.0f, 1.0f));
  JPH::BodyID CreateDynamicBox(simd_float3 position, simd_float3 half_extent, float mass);
  JPH::BodyID CreateDynamicCompound(simd_float3 position, std::span<const CollisionBox> boxes,
                                    float mass);
  JPH::BodyID CreateStaticCompound(simd_float3 position, std::span<const CollisionBox> boxes);

  bool IsSpawnAreaFree(simd_float3 position, simd_float3 half_extent, JPH::BodyID ignore = {},
                       float clearance = 0.2f) const;
  void DeactivateBody(JPH::BodyID id);

  void ResetBody(JPH::BodyID id, simd_float3 position);

  struct Pose {
    simd_float3 position;
    simd_quatf orientation;
  };
  Pose ReadPose(JPH::BodyID id) const;
  unsigned ActiveBodyCount() const;

  BodyState ReadState(JPH::BodyID id) const;

  void ApplyForceAt(JPH::BodyID id, simd_float3 force, simd_float3 world_point);

  // Call at a quiescent point after bulk insertion, never concurrently with Step.
  void OptimizeBroadPhase();

  void Step(const float &dt);

private:
  JPH::BodyID CreateCompound(simd_float3 position, std::span<const CollisionBox> boxes,
                             bool dynamic, float mass);
};
