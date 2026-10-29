#include "simulation/simulation.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
void Check(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void Equal(const BodyState &a, const BodyState &b) {
  Check(simd_all(a.position == b.position) && simd_all(a.center_off_mass == b.center_off_mass) &&
            simd_all(a.orientation.vector == b.orientation.vector) &&
            simd_all(a.linear_velocity == b.linear_velocity) &&
            simd_all(a.angular_velogity == b.angular_velogity),
        "Large pile worker full-state mismatch");
}
} // namespace

int main() {
  try {
    std::vector<std::vector<BodyState>> reference;
    for (int workers : {1, 6}) {
      Simulation sim(42, 10000, {workers, true, false});
      size_t sample = 0;
      for (int step = 0; step < 360; ++step) {
        // A dense no-wind pile, wind activation, then wind disabled again.
        sim.set_wind_enabled(step >= 240 && step < 300);
        sim.Update(1.f / 60);
        for (const auto &object : sim.get_objects())
          Check(!object->awaiting_spawn_ && !object->body_id_.IsInvalid() &&
                    std::isfinite(simd_reduce_add(object->position_)),
                "Large pile lost a body or produced nonfinite pose");
        if (step % 30 != 29)
          continue;
        auto states = sim.body_states();
        Check(states.size() == 10003, "Missing actual pile body state");
        if (workers == 1) {
          reference.push_back(std::move(states));
        } else {
          for (size_t i = 0; i < states.size(); ++i)
            Equal(states[i], reference[sample][i]);
          ++sample;
        }
      }
    }
    {
      PhysicsWorld world({6, true, false});
      world.CreateStaticBox({0, -.5f, 0}, {10, .5f, 10});
      const auto id = world.CreateDynamicBox({0, .5f, 0}, {.5f, .5f, .5f}, 1);
      for (int step = 0; step < 180; ++step)
        world.Step(1.f / 60);
      Check(world.ActiveBodyCount() == 0, "Resting body failed to sleep");
      const auto before = world.ReadState(id);
      Check(before.position.y > .45f, "Resting body penetrated floor");
      world.ApplyForceAt(id, {0, 200, 0}, before.center_off_mass);
      Check(world.ActiveBodyCount() == 1, "Force failed to wake sleeping body");
      world.Step(1.f / 60);
      Check(world.ReadState(id).position.y > before.position.y + .01f,
            "Woken body failed to respond to force");
    }
    std::puts("PASS: 10k pile, 360 steps per worker config, 12 full-state snapshots "
              "(1 vs 6 workers), wind on/off, full population, native sleep/wake and floor");
  } catch (const std::exception &error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
