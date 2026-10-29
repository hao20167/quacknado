#include "simulation/spawn_position.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <stdexcept>

namespace quacknado {
RespawnGrid::RespawnGrid(simd_float3 max_extent, float bottom, uint64_t &sequence)
    : extent_(max_extent), bottom_(bottom), sequence_(sequence) {
  if (!simd_all(max_extent > 0) || !std::isfinite(simd_reduce_add(max_extent)) ||
      2 * max_extent.x > 59 || 2 * max_extent.z > 59)
    throw std::invalid_argument("Invalid respawn grid extent");
  columns_ = static_cast<size_t>(59.4f / (2 * extent_.x + .4f));
  rows_ = static_cast<size_t>(59.4f / (2 * extent_.z + .4f));
  const size_t cells = columns_ * rows_;
  stride_ = std::max(size_t{1}, static_cast<size_t>(cells * .61803398875));
  while (std::gcd(stride_, cells) != 1)
    ++stride_;
}

simd_float3 RespawnGrid::Next(simd_float3 h) {
  if (!simd_all(h > 0) || !simd_all(h <= extent_))
    throw std::invalid_argument("Object exceeds respawn grid extent");
  const size_t cells = columns_ * rows_;
  const size_t slot = ((sequence_++ % cells) * stride_ + cells / 2) % cells;
  const size_t layer = issued_++ / cells;
  return {(static_cast<float>(slot % columns_) - (columns_ - 1) * .5f) *
              (2 * extent_.x + .4f),
          bottom_ + layer * (2 * extent_.y + .4f) + h.y,
          (static_cast<float>(slot / columns_) - (rows_ - 1) * .5f) *
              (2 * extent_.z + .4f)};
}

simd_float3 SpawnShelf::Next(simd_float3 h) {
  constexpr float gap = 0.4f;
  const auto size = 2 * h;
  if (!simd_all(h > 0) || !std::isfinite(h.x + h.y + h.z) || size.x > 59 || size.z > 59)
    throw std::invalid_argument("Invalid shelf spawn extent");
  if (x_ + size.x > 29.5f) {
    x_ = -29.5f;
    z_ += row_depth_ + gap;
    row_depth_ = 0;
  }
  if (z_ + size.z > 29.5f) {
    x_ = z_ = -29.5f;
    bottom_ += layer_height_ + gap;
    row_depth_ = layer_height_ = 0;
  }
  const simd_float3 position{x_ + h.x, bottom_ + h.y, z_ + h.z};
  x_ += size.x + gap;
  row_depth_ = std::max(row_depth_, size.z);
  layer_height_ = std::max(layer_height_, size.y);
  return position;
}

std::optional<simd_float3> FindSpawnPosition(const PhysicsWorld &physics, simd_float3 half_extent,
                                             std::mt19937_64 &rng, JPH::BodyID ignore,
                                             std::optional<simd_float3> preferred) {
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
        std::abs(candidate.z) + half_extent.z + clearance >= 30)
      continue;
    if (physics.IsSpawnAreaFree(candidate, half_extent, ignore, clearance))
      return candidate;
  }
  return std::nullopt;
}

} // namespace quacknado
