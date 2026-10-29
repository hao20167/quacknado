#include "simulation/simulation.hpp"
#include "simulation/physics/worker_policy.hpp"
#include <cstdio>
#include <stdexcept>
#include <vector>
namespace {
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Equal(const BodyState &a, const BodyState &b) {
  Check(simd_all(a.position == b.position), "Physics worker position mismatch");
  Check(simd_all(a.center_off_mass == b.center_off_mass), "Physics worker COM mismatch");
  Check(simd_all(a.orientation.vector == b.orientation.vector), "Physics worker rotation mismatch");
  Check(simd_all(a.linear_velocity == b.linear_velocity),
        "Physics worker linear velocity mismatch");
  Check(simd_all(a.angular_velogity == b.angular_velogity),
        "Physics worker angular velocity mismatch");
}
void CollisionInvariants(int workers, bool dynamic_collision) {
  PhysicsWorld world({workers, dynamic_collision, true});
  world.CreateStaticBox({0, -.5f, 0}, {10, .5f, 10});
  auto a = world.CreateDynamicBox({-.4f, 3, 0}, {.5f, .5f, .5f}, 1);
  auto b = world.CreateDynamicBox({.4f, 3, 0}, {.5f, .5f, .5f}, 1);
  unsigned dynamic = 0, statics = 0;
  for (int i = 0; i < 180; ++i) {
    world.Step(1.f / 60);
    auto c = world.Contacts();
    dynamic += c.dynamic;
    statics += c.static_contacts;
  }
  Check(statics > 0, "Missing floor contacts");
  Check(dynamic_collision ? dynamic > 0 : dynamic == 0, "Dynamic collision filtering incorrect");
  Check(world.ReadPose(a).position.y > .45f && world.ReadPose(b).position.y > .45f,
        "Bodies fell through floor");
}
} // namespace
int main() {
  try {
    Check(quacknado::DefaultPhysicsWorkers(10000, 0) == 0, "Unknown CPU must use serial");
    Check(quacknado::DefaultPhysicsWorkers(10000, 2) == 0, "Small CPU must use serial");
    Check(quacknado::DefaultPhysicsWorkers(1000, 10) == 1, "Small scene policy changed");
    Check(quacknado::DefaultPhysicsWorkers(6000, 10) == 2, "Medium pile lacks workers");
    Check(quacknado::DefaultPhysicsWorkers(10000, 10) == 6, "Large pile lacks workers");
    Check(quacknado::DefaultPhysicsWorkers(10000, 4) == 2, "Policy oversubscribed small CPU");
    for (auto seed : {42, 2026})
      for (size_t n : {size_t(100), size_t(600), size_t(1000)}) {
        std::vector<std::vector<BodyState>> reference;
        std::vector<unsigned> active;
        for (int config : {0, 1, 2, 4, 6, 11, 12}) {
          int workers = config >= 10 ? config - 10 : config;
          Simulation simulation(seed, n, {workers, true, false, config >= 10});
          simulation.set_wind_enabled(true);
          for (int step = 0; step < 120; ++step) {
            simulation.set_tornado_direction({step < 60 ? 1.f : -1.f, 0, 0});
            simulation.Update(1.f / 60);
            auto states = simulation.body_states();
            if (!workers) {
              reference.push_back(states);
              active.push_back(simulation.active_body_count());
              continue;
            }
            Check(states.size() == reference[step].size(),
                  "Physics worker placed body count mismatch");
            Check(simulation.active_body_count() == active[step],
                  "Physics worker active count mismatch");
            for (size_t i = 0; i < states.size(); ++i)
              Equal(states[i], reference[step][i]);
          }
        }
      }
    for (int workers : {0, 2, 4})
      for (bool collision : {false, true})
        CollisionInvariants(workers, collision);
    std::puts(
        "PASS: full-state serial/Jolt worker trajectories (2 seeds, 100/600/1000 objects, 120 "
        "steps, including worker QoS); floor/dynamic filtering invariants");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
