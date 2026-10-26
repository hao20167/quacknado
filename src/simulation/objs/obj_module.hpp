#pragma once

#include <simd/simd.h>

class IObjModule {
public:
  simd_float3 position_ {5, 0, 0};
  simd_float3 velocity_ {0, 0, 0};

  simd_quatf orientation_ = simd_quaternion(0.0f, 0.0f, 0.0f, 1.0f);
  simd_float3 angular_velocity_ {0, 0, 0};

  float mass_ = 1.0f;
  float size_ = 2.0f;
};
