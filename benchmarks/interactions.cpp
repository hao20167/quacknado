#include "simulation/simulation.hpp"
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/Profiler.h>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <cmath>
#include <string>
int main(int argc, char **argv) {
  using namespace JPH;
  try {
    size_t n = argc > 1 ? std::stoul(argv[1]) : 600;
    int workers = argc > 2 ? std::stoi(argv[2]) : 0;
    bool collision = argc > 3 ? std::stoi(argv[3]) != 0 : true;
    bool telemetry = argc > 4 ? std::stoi(argv[4]) != 0 : false;
    bool profile = argc > 5 ? std::stoi(argv[5]) != 0 : false;
    bool qos = argc > 6 ? std::stoi(argv[6]) != 0 : false;
    bool wind = argc > 7 ? std::stoi(argv[7]) != 0 : true;
    int steps = argc > 8 ? std::stoi(argv[8]) : 360;
    if (steps < 1 || steps > 10000)
      throw std::invalid_argument("steps must be 1..10000");
    JPH::RegisterDefaultAllocator();
    if (profile) {
      JPH_PROFILE_START("interaction-main");
    }
    {
      Simulation sim(42, n, {workers, collision, telemetry, qos});
      sim.profiling = true;
      sim.set_wind_enabled(wind);
      std::puts(
          "n,workers,qos,collisions,telemetry,step,update_ns,wind_ns,physics_ns,active,dynamic_"
          "manifolds,static_manifolds,checksum,wind_enabled,sync_ns,spawn_ns,respawned,placed,pending");
      for (int step = 0; step < steps; ++step) {
        sim.set_tornado_direction({(step / 60) % 2 ? -1.f : 1.f, 0, 0});
        auto begin = std::chrono::steady_clock::now();
        sim.Update(1.f / 60);
        auto end = std::chrono::steady_clock::now();
        if (profile) {
          if (step == 180) {
            JPH_PROFILE_DUMP("interaction");
          }
          JPH_PROFILE_NEXTFRAME();
        }
        // Preserve the historical CLI's 60 warmup / 300 measured rows.
        // Supplying wind explicitly requests the full transient trace instead.
        if (argc <= 7 && step < 60)
          continue;
        double sum = 0;
        size_t placed = 0;
        for (const auto &o : sim.get_objects())
          if (!o->awaiting_spawn_) {
            if (o->body_id_.IsInvalid() || !std::isfinite(simd_reduce_add(o->position_)))
              throw std::runtime_error("Invalid placed body");
            ++placed;
            sum += o->position_.x + o->position_.y + o->position_.z;
          }
        if (n > 1000 && placed != n + 3)
          throw std::runtime_error("Large scene lost body population");
        auto c = sim.contacts();
        std::printf("%zu,%d,%d,%d,%d,%d,%.0f,%.0f,%.0f,%u,%u,%u,%.17g,%d,%.0f,%.0f,%zu,%zu,%zu\n", n, workers, int(qos),
                    int(collision), int(telemetry), step,
                    std::chrono::duration<double, std::nano>(end - begin).count(),
                    sim.timings.wind_ns, sim.timings.physics_ns, sim.active_body_count(), c.dynamic,
                    c.static_contacts, sum, int(wind), sim.timings.sync_ns, sim.timings.spawn_ns,
                    sim.respawned_last_update(), placed, sim.get_objects().size() - placed);
        std::fflush(stdout);
      }
    }
    if (profile) {
      JPH_PROFILE_END();
    }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
