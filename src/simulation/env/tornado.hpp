#pragma once 

#include <simd/simd.h>

#include "simulation/objs/obj_module.hpp"

class Tornado {
private:
  simd_float3 center_ {0, 0, 0};
  float radius_ = 40.0f;
  float tangential_speed_ = 50.0f;
  float suction_speed_ = 20.0f;
  float lift_speed_ = 15.0f;
public:
  [[nodiscard]]
  simd_float3 VelocityAt(const simd_float3& position) const;
  [[nodiscard]]
  simd_float3 ForceAt(const simd_float3& position, const simd_float3& velocity, const float& mass) const;
  void UpdateObj(IObjModule* obj, const float& dt);
};
