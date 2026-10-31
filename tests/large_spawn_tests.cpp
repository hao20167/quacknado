#include "simulation/simulation.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <unordered_set>
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
int main(int argc, char **argv) {
  try {
    size_t n = argc > 1 ? std::stoul(argv[1]) : 10000;
    int steps = argc > 2 ? std::stoi(argv[2]) : 360;
    unsigned seed = argc > 3 ? std::stoul(argv[3]) : 42;
    auto start = std::chrono::steady_clock::now();
    Simulation simulation(seed, n, {1, true, false});
    simulation.set_wind_enabled(true);
    simulation.profiling = true;
    const auto &objects = simulation.get_objects();
    Check(objects.size() == n + 3, "Wrong requested scene population");
    struct Bounds {
      simd_float3 p, h;
    };
    std::vector<Bounds> initial;
    std::unordered_set<unsigned> ids;
    float top = 0;
    for (size_t i = 0; i < objects.size(); ++i) {
      const auto &o = *objects[i];
      Check(!o.awaiting_spawn_ && !o.body_id_.IsInvalid(), "Scene deferred an initial object");
      Check(ids.insert(o.body_id_.GetIndexAndSequenceNumber()).second, "Duplicate body ID");
      auto r = simd_matrix3x3(o.orientation_);
      auto h = simd_abs(r.columns[0]) * o.half_extent_.x +
               simd_abs(r.columns[1]) * o.half_extent_.y +
               simd_abs(r.columns[2]) * o.half_extent_.z;
      if (i)
        initial.push_back({o.position_, h}); // Props intentionally touch the floor.
      top = std::max(top, o.position_.y + h.y);
    }
    std::sort(initial.begin(), initial.end(),
              [](const auto &a, const auto &b) { return a.p.x - a.h.x < b.p.x - b.h.x; });
    for (size_t i = 0; i < initial.size(); ++i)
      for (size_t j = i + 1; j < initial.size(); ++j) {
        const auto &a = initial[i], &b = initial[j];
        if (b.p.x - b.h.x > a.p.x + a.h.x + .19f)
          break;
        Check(simd_any(simd_abs(a.p - b.p) > a.h + b.h + .19f),
              "Overlapping initial large-scene placement");
      }
    Check(simulation.body_states().size() == n + 3, "Missing actual physics state");
    double init_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::puts("step,requested,placed,pending,body_states,active,respawned,physics_ns,init_and_"
              "check_ms,initial_top");
    auto report = [&](int step) {
      size_t placed = 0;
      for (const auto &o : objects) {
        if (!o->awaiting_spawn_ && !o->body_id_.IsInvalid())
          ++placed;
        Check(std::isfinite(o->position_.x + o->position_.y + o->position_.z),
              "Nonfinite body pose");
      }
      Check(placed == n + 3, "Large scene lost objects to deferred spawn");
      auto count = simulation.body_states().size();
      Check(count == n + 3, "Large scene lost actual physics states");
      std::printf("%d,%zu,%zu,%zu,%zu,%u,%zu,%.0f,%.3f,%.3f\n", step, n, placed,
                  objects.size() - placed, count, simulation.active_body_count(),
                  simulation.respawned_last_update(), simulation.timings.physics_ns, init_ms, top);
      std::fflush(stdout);
    };
    report(0);
    for (int step = 1; step <= steps; ++step) {
      simulation.set_tornado_direction({((step - 1) / 60) % 2 ? -1.f : 1.f, 0, 0});
      simulation.Update(1.f / 60);
      report(step);
    }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: %s\n", e.what());
    return 1;
  }
}
