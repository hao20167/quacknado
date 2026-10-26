#include "simulation/env/tornado.hpp"

#include <cassert>
#include <cmath>

void test_tornado() {
  Tornado tornado;

  simd_float3 center = tornado.VelocityAt({0, 0, 0});
  assert(std::isfinite(center.x));
  assert(std::isfinite(center.y));
  assert(std::isfinite(center.z));

  simd_float3 outside = tornado.VelocityAt({20, 0, 0});
  assert(outside.x == 0);
  assert(outside.y == 0);
  assert(outside.z == 0);

  simd_float3 inside = tornado.VelocityAt({5, 0, 0});
  assert(inside.x < 0); // sucked up
  assert(inside.x > 0); // up
  assert(inside.z > 0); // rorating
}
