#include "simulation/simulation.hpp"
#include "simulation/spawn_position.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
void Check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

void CheckPhysicsAndPlacement() {
  PhysicsWorld world;
  world.CreateStaticBox(simd_float3{0, -0.5f, 0}, simd_float3{50, 0.5f, 50});
  House house(simd_float3{-8, 2, -6});
  PChair chair(simd_float3{0, 1, -2});
  const auto house_id = world.CreateStaticCompound(house.position_, house.collision_boxes_);
  const auto chair_id = world.CreateStaticBox(chair.position_, chair.half_extent_);
  world.ApplyForceAt(house_id, simd_float3{100000, 100000, 0}, house.position_);
  world.ApplyForceAt(chair_id, simd_float3{100000, 100000, 0}, chair.position_);
  const auto falling = world.CreateDynamicBox(simd_float3{0, 6, -2}, simd_float3{0.5f, 0.5f, 0.5f}, 50);
  for (int i = 0; i < 300; ++i) world.Step(1.0f / 60);
  Check(simd_length(world.ReadState(house_id).position - house.position_) < 1e-6f, "House moved");
  Check(simd_length(world.ReadState(chair_id).position - chair.position_) < 1e-6f, "Chair moved");
  Check(world.ReadState(falling).position.y > 2, "Dynamic body fell through the static chair");
  Check(!world.IsSpawnAreaFree(chair.position_, chair.half_extent_), "Chair does not reserve its space");

  const auto body = world.CreateDynamicBox(simd_float3{15, 4, 0}, simd_float3{1, 1, 1}, 1);
  Check(world.IsSpawnAreaFree(simd_float3{15, 4, 0}, simd_float3{1, 1, 1}, body), "Respawn did not ignore its own body");
  world.ResetBody(body, simd_float3{12, 4, 3});
  Check(!world.IsSpawnAreaFree(simd_float3{12, 4, 3}, simd_float3{1, 1, 1}), "Reset did not immediately reserve its destination");

  std::mt19937_64 rng(42);
  const simd_float3 size {0.5f, 0.5f, 0.5f};
  std::vector<JPH::BodyID> spawned;
  for (int i = 0; i < 100; ++i) {
    auto p = quacknado::FindSpawnPosition(world, size, rng);
    Check(p.has_value(), "Could not find space for small objects");
    Check(p->y - size.y > 0.2f, "Spawn clips the floor");
    Check(world.IsSpawnAreaFree(*p, size), "Spawn overlaps an existing body");
    spawned.push_back(world.CreateDynamicBox(*p, size, 1));
    Check(!world.IsSpawnAreaFree(*p, size), "New spawn did not reserve space immediately");
  }
  auto tall = quacknado::FindSpawnPosition(world, simd_float3{2, 4, 2}, rng);
  Check(!tall || tall->y > 4.2f, "Tall model spawned in the floor");

  for (int i = 0; i < 10; ++i) {
    auto p = quacknado::FindSpawnPosition(world, size, rng, spawned[i]);
    Check(p.has_value(), "Could not find a respawn destination");
    world.ResetBody(spawned[i], *p);
    Check(!world.IsSpawnAreaFree(*p, size), "Same-frame respawn did not reserve its destination");
  }

  const auto blocker = world.CreateDynamicBox(simd_float3{0, 5, 0}, simd_float3{40, 20, 40}, 1);
  Check(!quacknado::FindSpawnPosition(world, size, rng), "Full region did not defer spawn");
  world.ResetBody(blocker, simd_float3{1000, 5, 0});
  Check(quacknado::FindSpawnPosition(world, size, rng).has_value(), "Retry did not succeed after space became available");
}

void CheckScene() {
  Simulation simulation(1234);
  const auto& objects = simulation.get_objects();
  Check(objects.size() == 204, "Expected floor, fixed house/chair, dirt house and 200 objects");
  const IObjModule* chair = nullptr;
  const IObjModule* house = nullptr;
  const IObjModule* dirt = nullptr;
  for (const auto& object : objects) {
    if (object->name_ == "pchair") chair = object.get();
    if (object->name_ == "house") house = object.get();
    if (object->name_ == "dirt_house") dirt = object.get();
  }
  Check(chair && house && dirt, "Missing a scene prop");
  Check(chair->is_static_ && house->is_static_ && !dirt->is_static_, "Wrong body types");
  Check(!chair->affected_by_wind_ && !house->affected_by_wind_ && dirt->affected_by_wind_, "Wrong wind flags");
  for (size_t i = 1; i < objects.size(); ++i) {
    if (objects[i]->awaiting_spawn_) continue;
    Check(!objects[i]->body_id_.IsInvalid(), "Placed object has no body");
    for (size_t j = 1; j < i; ++j) {
      if (objects[j]->awaiting_spawn_) continue;
      auto delta = simd_abs(objects[i]->position_ - objects[j]->position_);
      auto extent = objects[i]->half_extent_ + objects[j]->half_extent_ + 0.19f;
      Check(simd_any(delta > extent), "Initial spawn overlaps another object");
    }
  }
  const auto chair_position = chair->position_;
  const auto house_position = house->position_;
  const auto dirt_position = dirt->position_;
  simulation.set_wind_enabled(true);
  for (int i = 0; i < 300; ++i) simulation.Update(1.0f / 60);
  Check(simd_length(chair->position_ - chair_position) < 1e-6f, "Tornado moved chair");
  Check(simd_length(simd_act(chair->orientation_, simd_float3{0, 0, 1}) - simd_float3{0, 0, -1}) < 1e-6f,
      "Chair lost its 180-degree rotation after physics update");
  Check(simd_length(house->position_ - house_position) < 1e-6f, "Tornado moved house");
  Check(simd_length(dirt->position_ - dirt_position) > 0.1f, "Dirt house did not move");
  for (const auto& object : objects)
    Check(std::isfinite(object->position_.y), "Simulation produced invalid positions");
}
}

int main() {
  try {
    CheckPhysicsAndPlacement();
    CheckScene();
    std::puts("PASS: fixed props, dynamic dirt house, overlap-free placement, self exclusion, same-tick reservations and deferred retries");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
