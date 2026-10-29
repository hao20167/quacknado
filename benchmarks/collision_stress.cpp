#include "simulation/physics/physics_world.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
int main(int argc, char **argv) {
  try {
    int workers = argc > 1 ? std::stoi(argv[1]) : 1;
    bool telemetry = argc > 2 ? std::stoi(argv[2]) != 0 : false;
    int steps = argc > 3 ? std::stoi(argv[3]) : 360;
    if (steps < 1 || steps > 1200)
      throw std::invalid_argument("steps 1..1200");
    PhysicsWorld world({workers, true, telemetry});
    world.CreateStaticBox({0, -.5f, 0}, {30, .5f, 30});
    std::vector<JPH::BodyID> ids;
    for (int y = 0; y < 10; ++y)
      for (int z = 0; z < 10; ++z)
        for (int x = 0; x < 10; ++x) {
          auto id = world.CreateDynamicBox(
              {(x - 4.5f) * .5625f, .28125f + y * .5625f, (z - 4.5f) * .5625f}, {.25f, .25f, .25f},
              1);
          if (id.IsInvalid())
            throw std::runtime_error("dense fixture body allocation failed");
          ids.push_back(id);
        }
    world.OptimizeBroadPhase();
    std::puts(
        "step,workers,telemetry,physics_ns,active,dynamic_manifolds,static_manifolds,checksum");
    for (int step = 0; step < steps; ++step) {
      // Coherent alternating lateral forcing keeps 1000 boxes active in a dense contact pile.
      // Force assembly, body reads and validation stay outside the physics timer.
      for (auto id : ids) {
        auto state = world.ReadState(id);
        world.ApplyForceAt(id, {(step / 30) % 2 ? -8.f : 8.f, 0, 0}, state.center_off_mass);
      }
      auto begin = std::chrono::steady_clock::now();
      world.Step(1.f / 60);
      auto elapsed =
          std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begin)
              .count();
      double checksum = 0;
      for (auto id : ids) {
        auto s = world.ReadState(id);
        if (!std::isfinite(s.position.x + s.position.y + s.position.z) || s.position.y < -.05f ||
            s.position.y > 25 || simd_length(s.linear_velocity) > 50 ||
            std::abs(simd_length(s.orientation.vector) - 1) > 1e-3f)
          throw std::runtime_error("dense contact finite/floor/speed/quaternion invariant failed");
        checksum += s.position.x + s.position.y + s.position.z;
      }
      auto c = world.Contacts();
      std::printf("%d,%d,%d,%.0f,%u,%u,%u,%.17g\n", step, workers, int(telemetry), elapsed,
                  world.ActiveBodyCount(), c.dynamic, c.static_contacts, checksum);
    }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
