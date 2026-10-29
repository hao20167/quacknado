#pragma once

#include "simulation/physics/physics_world.hpp"
#include <optional>
#include <random>

namespace quacknado {
// A shelf packs axis-aligned boxes in rows, then layers, inside the existing floor.
// Callers choose a bottom above existing bodies; newly issued slots cannot overlap.
class SpawnShelf {
  float x_ = -29.5f, z_ = -29.5f, bottom_;
  float row_depth_ = 0, layer_height_ = 0;

public:
  explicit SpawnShelf(float bottom) : bottom_(bottom) {}
  simd_float3 Next(simd_float3 half_extent);
};

// Visit every cell once before adding a layer. The sequence survives physics steps,
// so a small respawn batch cannot repeatedly restart at the same floor corner.
class RespawnGrid {
  simd_float3 extent_;
  float bottom_;
  size_t columns_, rows_, stride_, issued_ = 0;
  uint64_t &sequence_;

public:
  RespawnGrid(simd_float3 max_extent, float bottom, uint64_t &sequence);
  simd_float3 Next(simd_float3 half_extent);
};

std::optional<simd_float3> FindSpawnPosition(const PhysicsWorld &physics, simd_float3 half_extent,
                                             std::mt19937_64 &rng, JPH::BodyID ignore = {},
                                             std::optional<simd_float3> preferred = {});
} // namespace quacknado
