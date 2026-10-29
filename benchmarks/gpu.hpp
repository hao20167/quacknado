#pragma once
#include "wind.hpp"
#include <memory>
class GPU {
  struct Impl;
  std::unique_ptr<Impl> impl_;

public:
  explicit GPU(size_t n);
  ~GPU();
  double run(std::span<const bench::Sample>, std::span<simd_float3>);
};
void render_benchmark(size_t count, int repetitions);

void glb_benchmark(size_t count, int repetitions, const char *model_name);
