#include "simulation/spawn_position.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace quacknado {
std::optional<simd_float3> FindSpawnPosition(const PhysicsWorld& physics, simd_float3 half_extent,
    std::mt19937_64& rng, JPH::BodyID ignore, std::optional<simd_float3> preferred) {
  constexpr int max_attempts = 32;
  constexpr float clearance = 0.2f;
  std::uniform_real_distribution<float> angle_distribution(0, 2.0f * std::numbers::pi_v<float>);
  std::uniform_real_distribution<float> radius_squared_distribution(25.0f, 225.0f);
  std::uniform_real_distribution<float> height_distribution(1.0f, 8.0f);
  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    simd_float3 candidate = preferred.value_or(simd_float3{0, 0, 0});
    if (!preferred || attempt != 0) {
      const float angle = angle_distribution(rng);
      const float radius = std::sqrt(radius_squared_distribution(rng));
      candidate = {1.5f * radius * std::cos(angle),
          std::max(height_distribution(rng), half_extent.y + clearance + 0.05f),
          radius * std::sin(angle)};
    }
    if (candidate.y - half_extent.y <= clearance ||
        std::abs(candidate.x) + half_extent.x + clearance >= 30 ||
        std::abs(candidate.z) + half_extent.z + clearance >= 30) continue;
    if (physics.IsSpawnAreaFree(candidate, half_extent, ignore, clearance))
      return candidate;
  }
  return std::nullopt;
}

}
