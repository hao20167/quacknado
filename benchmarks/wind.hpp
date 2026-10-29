#pragma once
#include "core/worker_pool.hpp"
#include "simulation/env/tornado.hpp"
#include <algorithm>
#include <arm_neon.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

namespace bench {
struct Sample {
  simd_float3 p, v;
};
struct SoA {
  std::vector<float> x, y, z, vx, vy, vz;
  explicit SoA(std::span<const Sample> a) {
    for (auto s : a) {
      x.push_back(s.p.x);
      y.push_back(s.p.y);
      z.push_back(s.p.z);
      vx.push_back(s.v.x);
      vy.push_back(s.v.y);
      vz.push_back(s.v.z);
    }
  }
};
inline void scalar(const Tornado &t, std::span<const Sample> a, std::span<simd_float3> out,
                   bool ref = false) {
  for (size_t i = 0; i < a.size(); ++i)
    out[i] = ref ? t.ReferenceForceAt(a[i].p, a[i].v) : t.ForceAt(a[i].p, a[i].v);
}
inline void soa(const Tornado &t, const SoA &a, std::span<simd_float3> out) {
  for (size_t i = 0; i < out.size(); ++i)
    out[i] = t.ForceAt({a.x[i], a.y[i], a.z[i]}, {a.vx[i], a.vy[i], a.vz[i]});
}
// Four independent samples per vector; exp remains scalar libm per lane.
inline void neon(const Tornado &tornado, const SoA &a, std::span<simd_float3> out, size_t begin = 0,
                 size_t end = SIZE_MAX, bool cull = false) {
  end = std::min(end, out.size());
  using V = simd_float4;
  auto smooth = [](float low, float high, V x) {
    V t = simd_clamp((x - low) / (high - low), V{0, 0, 0, 0}, V{1, 1, 1, 1});
    return t * t * (3 - 2 * t);
  };
  size_t i = begin;
  for (; i + 4 <= end; i += 4) {
    V x = vld1q_f32(a.x.data() + i) - tornado.center().x;
    V y = vld1q_f32(a.y.data() + i) - tornado.center().y;
    V z = vld1q_f32(a.z.data() + i) - tornado.center().z;
    V u = y / 30.0f, radius = 10 + 20 * u;
    V r = vsqrtq_f32(x * x + z * z), t = r / radius;
    if (cull && simd_all((y < 0) | (y >= 30) | (t >= 1.5f))) {
      for (int k = 0; k < 4; ++k)
        out[i + k] = {};
      continue;
    }
    V inv = 1 / simd_max(r, V{1e-5f, 1e-5f, 1e-5f, 1e-5f});
    for (int k = 0; k < 4; ++k)
      if (r[k] <= 1e-5f)
        inv[k] = 0;
    V top = smooth(.7f, .95f, u), side = 1 - smooth(1, 1.5f, t), ti = 1 - smooth(.95f, 1, u);
    V swirl;
    for (int k = 0; k < 4; ++k)
      swirl[k] = t[k] <= 1 ? t[k] : 1 / t[k];
    V rotation = 50 * swirl * (1 - .5f * top),
      radial = (-100 * (1 - top) + 25 * top) * simd_min(t, V{1, 1, 1, 1});
    V fx = (-z * inv * rotation + x * inv * radial - vld1q_f32(a.vx.data() + i)) * 3 * side * ti;
    V fz = (x * inv * rotation + z * inv * radial - vld1q_f32(a.vz.data() + i)) * 3 * side * ti;
    constexpr float heights[]{0, .2f, .5f, .9f, 1}, strengths[]{.12f, .45f, 1, .25f, 0};
    for (int k = 0; k < 4; ++k) {
      if (y[k] < 0 || y[k] >= 30 || t[k] >= 1.5f) {
        out[i + k] = {};
        continue;
      }
      float lift = 0;
      for (int j = 1; j < 5; ++j)
        if (u[k] <= heights[j]) {
          float b = std::clamp((u[k] - heights[j - 1]) / (heights[j] - heights[j - 1]), 0.f, 1.f);
          b = b * b * (3 - 2 * b);
          lift = strengths[j - 1] + (strengths[j] - strengths[j - 1]) * b;
          break;
        }
      out[i + k] = {
          fx[k], std::exp(-t[k] * t[k]) * side[k] * (120 * lift - 6 * a.vy[i + k] * ti[k]), fz[k]};
    }
  }
  for (; i < end; ++i)
    out[i] = tornado.ForceAt({a.x[i], a.y[i], a.z[i]}, {a.vx[i], a.vy[i], a.vz[i]});
}
using Pool = quacknado::WorkerPool;
} // namespace bench
