#pragma once

#include <simd/simd.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

class IObjModule {
public:
  JPH::BodyID body_id_;
  simd_float3 position_ {0, 0, 0};
  simd_quatf orientation_ = simd_quaternion(0.0f, 0.0f, 0.0f, 1.0f);
  simd_float3 half_extent_ {1, 1, 1};
  float mass_ = 1.0f;
  bool affected_by_wind_ = true;

  virtual ~IObjModule() = default;
};
