#pragma once
#include "simulation/physics/physics_world.hpp"
#include <chrono>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <vector>
namespace bench {
struct Box {
  simd_float3 p, e;
};
inline bool overlaps(Box a, Box b) { return simd_all(simd_abs(a.p - b.p) <= a.e + b.e); }
struct Cell {
  int x, z;
  bool operator==(const Cell &) const = default;
};
struct Hash {
  size_t operator()(Cell c) const { return uint64_t(uint32_t(c.x)) * 0x9e3779b1u ^ uint32_t(c.z); }
};
class Grid {
  float size_;
  std::unordered_map<Cell, std::vector<size_t>, Hash> cells_;
  template <class F> void cells(Box b, F fn) const {
    for (int x = int(std::floor((b.p.x - b.e.x) / size_));
         x <= int(std::floor((b.p.x + b.e.x) / size_)); ++x)
      for (int z = int(std::floor((b.p.z - b.e.z) / size_));
           z <= int(std::floor((b.p.z + b.e.z) / size_)); ++z)
        fn(Cell{x, z});
  }

public:
  explicit Grid(float size) : size_(size) {}
  void build(const std::vector<Box> &boxes) {
    cells_.clear();
    for (size_t i = 0; i < boxes.size(); ++i)
      cells(boxes[i], [&](Cell c) { cells_[c].push_back(i); });
  }
  bool occupied(Box query, const std::vector<Box> &boxes) const {
    bool found = false;
    cells(query, [&](Cell c) {
      if (found)
        return;
      auto it = cells_.find(c);
      if (it != cells_.end())
        for (auto i : it->second)
          if (overlaps(query, boxes[i])) {
            found = true;
            break;
          }
    });
    return found;
  }
};
inline void spatial(size_t n, int reps, const std::string &dataset) {
  if (n > 1000)
    throw std::runtime_error("Spatial fixture limited to 1000 bodies by PhysicsWorld capacity");
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> d(-1, 1);
  std::vector<Box> boxes, queries;
  for (size_t i = 0; i < n; ++i) {
    float scale = dataset == "clustered" ? 5 : 50;
    Box b{{d(rng) * scale, 5 + d(rng) * 4, d(rng) * scale}, {.3f, .3f, .3f}};
    boxes.push_back(b);
  }
  for (int i = 0; i < 1000; ++i)
    queries.push_back({{d(rng) * 50, 5 + d(rng) * 4, d(rng) * 50}, {.5f, .5f, .5f}});
  std::vector<bool> expected;
  for (auto q : queries) {
    bool hit = false;
    for (auto b : boxes)
      if (overlaps(q, b)) {
        hit = true;
        break;
      }
    expected.push_back(hit);
  }
  for (float cell : {1.f, 4.f, 16.f}) {
    PhysicsWorld world;
    for (auto b : boxes)
      world.CreateStaticBox(b.p, b.e);
    Grid grid(cell);
    auto start = std::chrono::steady_clock::now();
    grid.build(boxes);
    auto end = std::chrono::steady_clock::now();
    std::printf("grid_build_%g,%s,%zu,0,%.0f,0,0\n", cell, dataset.c_str(), n,
                std::chrono::duration<double, std::nano>(end - start).count());
    for (int backend = 0; backend < 4; ++backend) {
      if (backend == 3) {
        start = std::chrono::steady_clock::now();
        world.OptimizeBroadPhase();
        end = std::chrono::steady_clock::now();
        std::printf("jolt_optimize_%g,%s,%zu,0,%.0f,0,0\n", cell, dataset.c_str(), n,
                    std::chrono::duration<double, std::nano>(end - start).count());
      }
      for (int r = -3; r < reps; ++r) {
        size_t hits = 0;
        start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < queries.size(); ++i) {
          auto q = queries[i];
          bool hit = false;
          if (backend == 0) {
            for (auto b : boxes)
              if (overlaps(q, b)) {
                hit = true;
                break;
              }
          } else if (backend == 1)
            hit = grid.occupied(q, boxes);
          else
            hit = !world.IsSpawnAreaFree(q.p, q.e, {}, 0);
          if (hit != expected[i])
            throw std::runtime_error("Spatial query mismatch");
          hits += hit;
        }
        end = std::chrono::steady_clock::now();
        if (r >= 0)
          std::printf("%s_%g,%s,%zu,%d,%.0f,0,%zu\n",
                      backend == 0   ? "linear"
                      : backend == 1 ? "grid"
                      : backend == 2 ? "jolt_before"
                                     : "jolt_after",
                      cell, dataset.c_str(), n, r,
                      std::chrono::duration<double, std::nano>(end - start).count(), hits);
      }
    }
  }
}
} // namespace bench
