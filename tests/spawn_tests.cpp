#include "simulation/simulation.hpp"
#include "simulation/spawn_position.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

void CheckDistributedRespawn() {
  uint64_t sequence = 0;
  const simd_float3 extent{5, 2, 5};
  std::vector<simd_float3> positions;
  quacknado::RespawnGrid batch(extent, 10, sequence);
  for (int i = 0; i < 75; ++i) {
    const auto p = batch.Next(extent); // 25 cells, then two more layers.
    Check(std::abs(p.x) + extent.x <= 29.5f && std::abs(p.z) + extent.z <= 29.5f,
          "Respawn outside the floor");
    Check(p.y - extent.y >= 10, "Respawn below reserved ceiling");
    for (const auto &other : positions)
      Check(simd_any(simd_abs(p - other) > 2 * extent + .19f),
            "Respawn batch overlaps across a grid wrap");
    positions.push_back(p);
  }
  positions.clear();
  sequence = 0;
  unsigned quadrants = 0;
  for (int frame = 0; frame < 25; ++frame) {
    quacknado::RespawnGrid single(extent, 10, sequence);
    const auto p = single.Next(extent);
    for (const auto &other : positions)
      Check(simd_length(p - other) > .1f, "Sparse respawn restarted at the same cell");
    if (p.x != 0 && p.z != 0)
      quadrants |= 1u << ((p.x > 0 ? 1 : 0) + (p.z > 0 ? 2 : 0));
    positions.push_back(p);
  }
  Check(quadrants == 15, "Sparse respawn did not cover all floor quadrants");

  Simulation simulation(42, 1001);
  const auto &objects = simulation.get_objects();
  // Queue a burst explicitly, then ensure all slots are placed in the same step.
  for (size_t i = 3; i < 35; ++i)
    objects[i]->awaiting_spawn_ = true;
  simulation.Update(1.f / 60);
  Check(simulation.respawned_last_update() == 32, "Respawn burst was deferred");
  for (size_t i = 3; i < 35; ++i) {
    Check(!objects[i]->awaiting_spawn_, "Respawn left a queued object");
    for (size_t j = 3; j < i; ++j)
      Check(simd_any(simd_abs(objects[i]->position_ - objects[j]->position_) >
                     objects[i]->half_extent_ + objects[j]->half_extent_ + .19f),
            "Simulation placed overlapping respawns");
  }
  positions.clear();
  quadrants = 0;
  for (int frame = 0; frame < 32; ++frame) {
    objects[3]->awaiting_spawn_ = true;
    simulation.Update(1.f / 60);
    Check(simulation.respawned_last_update() == 1, "Sparse respawn was deferred");
    const auto p = objects[3]->position_;
    for (const auto &other : positions)
      Check(std::abs(p.x - other.x) + std::abs(p.z - other.z) > .1f,
            "Simulation restarted sparse respawn at the same cell");
    quadrants |= 1u << ((p.x > 0 ? 1 : 0) + (p.z > 0 ? 2 : 0));
    positions.push_back(p);
  }
  Check(quadrants == 15, "Simulation respawn stayed in one floor corner");
  Check(simulation.body_states().size() == 1004, "Respawn lost body states");
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
  const auto falling =
      world.CreateDynamicBox(simd_float3{0, 6, -2}, simd_float3{0.5f, 0.5f, 0.5f}, 50);
  for (int i = 0; i < 300; ++i)
    world.Step(1.0f / 60);
  Check(simd_length(world.ReadState(house_id).position - house.position_) < 1e-6f, "House moved");
  Check(simd_length(world.ReadState(chair_id).position - chair.position_) < 1e-6f, "Chair moved");
  Check(world.ReadState(falling).position.y > 2, "Dynamic body fell through the static chair");
  Check(!world.IsSpawnAreaFree(chair.position_, chair.half_extent_),
        "Chair does not reserve its space");

  {
    const auto pose = world.ReadPose(falling);
    const auto state = world.ReadState(falling);
    Check(simd_length(pose.position - state.position) == 0,
          "Pose position differs from full state");
    Check(simd_length(pose.orientation.vector - state.orientation.vector) == 0,
          "Pose rotation differs from full state");
  }
  const auto body = world.CreateDynamicBox(simd_float3{15, 4, 0}, simd_float3{1, 1, 1}, 1);
  Check(world.IsSpawnAreaFree(simd_float3{15, 4, 0}, simd_float3{1, 1, 1}, body),
        "Respawn did not ignore its own body");
  world.ResetBody(body, simd_float3{12, 4, 3});
  Check(!world.IsSpawnAreaFree(simd_float3{12, 4, 3}, simd_float3{1, 1, 1}),
        "Reset did not immediately reserve its destination");

  std::mt19937_64 rng(42);
  const simd_float3 size{0.5f, 0.5f, 0.5f};
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
  Check(quacknado::FindSpawnPosition(world, size, rng).has_value(),
        "Retry did not succeed after space became available");
}

void CheckScene() {
  Simulation simulation(1234, 600);
  const auto &objects = simulation.get_objects();
  Check(objects.size() == 603, "Expected floor, fixed house/chair, dirt house and 200 objects");
  const IObjModule *chair = nullptr;
  const IObjModule *house = nullptr;
  const IObjModule *dirt = nullptr;
  for (const auto &object : objects) {
    if (object->name_ == "pchair")
      chair = object.get();
    if (object->name_ == "house")
      house = object.get();
    if (object->name_ == "dirt_house")
      dirt = object.get();
  }
  Check(chair && dirt, "Missing a scene prop");
  Check(chair->is_static_ && !dirt->is_static_, "Wrong body types");
  Check(!chair->affected_by_wind_ && dirt->affected_by_wind_, "Wrong wind flags");
  for (size_t i = 1; i < objects.size(); ++i) {
    if (objects[i]->awaiting_spawn_)
      continue;
    Check(!objects[i]->body_id_.IsInvalid(), "Placed object has no body");
    for (size_t j = 1; j < i; ++j) {
      if (objects[j]->awaiting_spawn_)
        continue;
      auto delta = simd_abs(objects[i]->position_ - objects[j]->position_);
      auto extent = objects[i]->half_extent_ + objects[j]->half_extent_ + 0.19f;
      Check(simd_any(delta > extent), "Initial spawn overlaps another object");
    }
  }
  const auto chair_position = chair->position_;
  const auto chair_forward = simd_act(chair->orientation_, simd_float3{0, 0, 1});

  const auto dirt_position = dirt->position_;
  simulation.set_wind_enabled(true);
  for (int i = 0; i < 300; ++i)
    simulation.Update(1.0f / 60);
  Check(simd_length(chair->position_ - chair_position) < 1e-6f, "Tornado moved chair");
  Check(simd_length(simd_act(chair->orientation_, simd_float3{0, 0, 1}) - chair_forward) < 1e-6f,
        "Chair lost its initial rotation after physics update");

  Check(simd_length(dirt->position_ - dirt_position) > 0.1f, "Dirt house did not move");
  for (const auto &object : objects)
    Check(std::isfinite(object->position_.y), "Simulation produced invalid positions");
}
} // namespace

void CheckParallelTrajectory() {
  std::vector<std::vector<BodyState>> reference;
  for (int workers : {-1, 0, 1, 2, 4}) {
    Simulation simulation(42, 100);
    simulation.set_wind_enabled(true);
    simulation.set_wind_workers(workers < 0 ? 0 : workers);
    if (workers < 0)
      simulation.set_physics_access(false, true);
    for (int step = 0; step < 120; ++step) {
      simulation.set_tornado_direction(step < 60 ? simd_float3{1, 0, 0} : simd_float3{-1, 0, 0});
      simulation.Update(1.f / 60);
      auto states = simulation.body_states();
      if (workers < 0) {
        reference.push_back(states);
        continue;
      }
      Check(states.size() == reference[step].size(), "Parallel body count mismatch");
      for (size_t i = 0; i < states.size(); ++i) {
        const auto &a = states[i];
        const auto &b = reference[step][i];
        Check(simd_all(a.position == b.position), "Parallel position mismatch");
        Check(simd_all(a.center_off_mass == b.center_off_mass), "Parallel COM mismatch");
        Check(simd_all(a.orientation.vector == b.orientation.vector), "Parallel rotation mismatch");
        Check(simd_all(a.linear_velocity == b.linear_velocity), "Parallel velocity mismatch");
        Check(simd_all(a.angular_velogity == b.angular_velogity),
              "Parallel angular velocity mismatch");
      }
    }
  }
}

int main() {
  try {
    CheckDistributedRespawn();
    CheckPhysicsAndPlacement();
    CheckScene();
    CheckParallelTrajectory();
    std::puts("PASS: fixed props, dynamic dirt house, overlap-free placement, self exclusion, "
              "same-tick reservations and deferred retries");
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
