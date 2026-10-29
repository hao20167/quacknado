#pragma once

#include <algorithm>
#include <cstddef>

namespace quacknado {
// Small scenes retain the low-overhead policy. Large contact piles need enough
// workers for Jolt's large-island solver; reserve CPU capacity for the main/GPU path.
inline int DefaultPhysicsWorkers(size_t objects, unsigned hardware_threads) {
  if (hardware_threads < 4)
    return 0;
  const unsigned desired = objects >= 8192 ? 6 : objects >= 4096 ? 2 : 1;
  return static_cast<int>(std::min(desired, hardware_threads - 2));
}
} // namespace quacknado
