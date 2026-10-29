#include "wind.hpp"
#include <stdexcept>
int main() {
  Tornado t;
  for (size_t workers : {1, 2, 4}) {
    bench::Pool pool(workers);
    bool caught = false;
    try {
      pool.run(10, [](size_t, size_t) { throw std::runtime_error("expected worker failure"); });
    } catch (const std::runtime_error &) {
      caught = true;
    }
    if (!caught)
      throw std::runtime_error("Worker exception did not propagate");
    for (size_t n : {size_t(0), size_t(1), size_t(3), size_t(10003)}) {
      std::vector<bench::Sample> a(n, {{1, 10, -8}, {2, 3, 4}});
      std::vector<simd_float3> out(n), ref(n);
      bench::scalar(t, a, ref, true);
      for (int r = 0; r < 30; ++r) {
        pool.run(n, [&](size_t b, size_t e) {
          bench::scalar(t, std::span(a).subspan(b, e - b), std::span(out).subspan(b, e - b));
        });
        for (size_t i = 0; i < n; ++i)
          if (simd_length(out[i] - ref[i]) > 1e-3)
            throw std::runtime_error("Pool mismatch");
      }
    }
  }
}
