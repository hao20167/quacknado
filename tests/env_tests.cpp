#include "simulation/env/tornado.hpp"

#include <cmath>
#include <stdexcept>

void Check(bool value) {
  if (!value)
    throw std::runtime_error("Tornado invariant failed");
}

int main() {
  Tornado tornado;

  simd_float3 center = tornado.VelocityAt(tornado.center());
  Check(std::isfinite(center.x));
  Check(std::isfinite(center.y));
  Check(std::isfinite(center.z));

  simd_float3 outside = tornado.VelocityAt({20, 0, 0});
  Check(outside.x == 0);
  Check(outside.y == 0);
  Check(outside.z == 0);

  simd_float3 inside = tornado.VelocityAt(tornado.center() + simd_float3{5, 0, 0});
  Check(inside.x < 0);  // inward horizontal flow
  Check(inside.y == 0); // lift is applied by ForceAt, not VelocityAt
  Check(inside.z > 0);  // tangential rotation
  auto top = tornado.ForceAt(tornado.center() + simd_float3{0, 30, 0}, {1, 2, 3});
  Check(simd_length_squared(top) == 0);
  auto force = tornado.ForceAt({5, 4, 0}, {1, 2, 3});
  tornado.Move({3, 0, -7});
  Check(simd_length(force - tornado.ForceAt({8, 4, -7}, {1, 2, 3})) < 1e-5f);
}
