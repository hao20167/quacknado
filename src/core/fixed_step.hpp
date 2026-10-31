#pragma once
#include <algorithm>
#include <utility>

namespace quacknado {
// The game and realtime replay share the same fixed-step/catch-up policy.
// Preserve the existing 100 ms elapsed-time clamp; do not drop owed steps here.
class FixedStep {
  double accumulator_ = 0;

public:
  static constexpr double dt = 1.0 / 60.0;
  template <class Update> unsigned Advance(double elapsed, Update &&update) {
    accumulator_ += std::min(elapsed, 0.1);
    unsigned steps = 0;
    while (accumulator_ >= dt) {
      std::forward<Update>(update)(static_cast<float>(dt));
      accumulator_ -= dt;
      ++steps;
    }
    return steps;
  }
  double PendingSeconds() const { return accumulator_; }
};
} // namespace quacknado
