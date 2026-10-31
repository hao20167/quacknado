#include "core/fixed_step.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>
void Check(bool ok) {
  if (!ok)
    throw std::runtime_error("fixed-step clock/catch-up invariant");
}
int main() {
  try {
    quacknado::FixedStep clock;
    unsigned ticks = 0;
    auto update = [&](float dt) {
      Check(dt == float(1.0 / 60.0));
      ++ticks;
    };
    for (int frame = 0; frame < 120; ++frame)
      clock.Advance(1.0 / 120.0, update);
    Check(ticks == 60 && std::abs(clock.PendingSeconds()) < 1e-12);
    Check(clock.Advance(0, update) == 0);
    // Existing game policy: one long frame contributes at most 100 ms (six ticks).
    Check(clock.Advance(.25, update) == 6 && ticks == 66);
    Check(std::abs(clock.PendingSeconds()) < 1e-12);
    quacknado::FixedStep partial;
    unsigned partial_ticks = 0;
    for (int i = 0; i < 3; ++i)
      Check(partial.Advance(1.0 / 240.0, [&](float) { ++partial_ticks; }) == 0);
    Check(partial.Advance(1.0 / 240.0, [&](float) { ++partial_ticks; }) == 1 && partial_ticks == 1);
    std::puts("PASS: 60 Hz simulation, 120 Hz rendering, fractional remainder and original 100 ms "
              "catch-up clamp");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
