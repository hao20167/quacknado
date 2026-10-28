#include "simulation/env/tornado.hpp"

#include <cstdio>
#include <stdexcept>

namespace {
void Check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

void CheckTornadoMovement() {
  Tornado tornado;
  const simd_float3 point {3, 2, 4};
  const simd_float3 velocity {1, -2, 3};
  const auto initial_flow = tornado.VelocityAt(point);
  const auto initial_force = tornado.ForceAt(point, velocity);
  Check(simd_length(initial_force) > 1.0f, "Expected a nonzero tornado force");
  tornado.Move(simd_float3{100, 50, -100});
  const auto translated_point = point + simd_float3{100, 0, -100};
  Check(simd_length(tornado.VelocityAt(translated_point) - initial_flow) < 1e-4f,
      "Wind field did not follow the tornado center on XZ");
  Check(simd_length(tornado.ForceAt(translated_point, velocity) - initial_force) < 1e-4f,
      "Force field did not follow the tornado center");
  Check(simd_length(tornado.ForceAt(point, velocity)) < 1e-6f,
      "Tornado still influences its old location");
  tornado.Move(simd_float3{0, 0, 0});
  Check(simd_length(tornado.VelocityAt(translated_point) - initial_flow) < 1e-4f,
      "Zero input moved the tornado");
}

}

int main() {
  try {
    CheckTornadoMovement();
    std::puts("PASS: tornado movement translates wind and force on XZ");
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
