#pragma once

#include <simd/simd.h>

class Tornado {
private:
  simd_float3 center_{0, 0, -8};
  float height_ = 30.0f;
  float bottom_radius_ = 10.0f;
  float top_radius_ = 30.0f;
  float tangential_speed_ = 50.0f;
  float suction_speed_ = 200.0f;
  float peak_lift_force_ = 120.0f;
  float vertical_drag_ = 6.0f;
  float outward_speed_ = 25.0f;

public:
  simd_float3 ReferenceVelocityAt(const simd_float3 &) const;
  simd_float3 ReferenceForceAt(const simd_float3 &, const simd_float3 &) const;

  simd_float3 center() const { return center_; }
  float bottom_radius() const { return bottom_radius_; }

  void Move(const simd_float3 &delta) {
    center_.x += delta.x;
    center_.z += delta.z;
  }

  [[nodiscard]] simd_float3 VelocityAt(const simd_float3 &position) const;
  [[nodiscard]] simd_float3 ForceAt(const simd_float3 &position, const simd_float3 &velocity) const;
};
