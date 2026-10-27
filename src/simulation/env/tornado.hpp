#pragma once 

#include <simd/simd.h>

#include "simulation/objs/obj_module.hpp"

class Tornado {
private:
  simd_float3 center_ {0, 0, 0};
  float height_ = 30.0f;
  float bottom_radius_ = 10.0f;
  float top_radius_ = 30.0f;
  float tangential_speed_ = 50.0f;
  float suction_speed_ = 50.0f;
  float lift_speed_ = 10.0f;
  float outward_speed_ = 25.0f;
public:
  [[nodiscard]]
  simd_float3 VelocityAt(const simd_float3& position) const;
  [[nodiscard]]
  simd_float3 ForceAt(const simd_float3& position, const simd_float3& velocity) const;
};
