#pragma once

#include <optional>
#include <random>
#include "simulation/physics/physics_world.hpp"

namespace quacknado {
std::optional<simd_float3> FindSpawnPosition(const PhysicsWorld& physics, simd_float3 half_extent,
    std::mt19937_64& rng, JPH::BodyID ignore = {}, std::optional<simd_float3> preferred = {});
}
