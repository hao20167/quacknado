#include "gpu.hpp"
#include "simulation/simulation.hpp"
#include "spatial.hpp"
#include "wind.hpp"
#include <chrono>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>
using Clock = std::chrono::steady_clock;
volatile double sink;
bool calibrated_measurements = false;
void check(std::span<const simd_float3> ref, std::span<const simd_float3> actual) {
  for (size_t i = 0; i < ref.size(); ++i)
    for (int k = 0; k < 3; ++k)
      if (!std::isfinite(actual[i][k]) ||
          std::abs(ref[i][k] - actual[i][k]) > 2e-4f + 2e-5f * std::abs(ref[i][k]))
        throw std::runtime_error("Numerical mismatch sample=" + std::to_string(i) +
                                 " component=" + std::to_string(k));
}
template <class F>
void measure(const char *backend, const char *dataset, size_t n, F fn, std::span<simd_float3> out,
             int reps) {
  for (int i = 0; i < 3; ++i)
    fn();
  size_t iterations = 1;
  auto batch = [&](size_t count) {
    double gpu = 0;
    auto begin = Clock::now();
    for (size_t i = 0; i < count; ++i) {
      gpu += fn();
      asm volatile("" : : "r"(out.data()) : "memory");
    }
    auto end = Clock::now();
    return std::pair{std::chrono::duration<double, std::nano>(end - begin).count(), gpu};
  };
  if (calibrated_measurements) {
    for (;;) {
      auto [elapsed, gpu] = batch(iterations);
      if (elapsed >= 5e6) {
        std::fprintf(stderr, "calibration,%s,%zu,%zu,%.0f\n", backend, n, iterations, elapsed);
        break;
      }
      if (iterations >= (1u << 20))
        throw std::runtime_error("Calibration failed to reach target duration");
      iterations *= 2;
    }
  }
  for (int r = 0; r < reps; ++r) {
    auto [elapsed, gpu] = batch(iterations);
    double sum = 0;
    for (auto v : out)
      sum += v.x + v.y + v.z;
    sink = sum;
    std::printf("%s,%s,%zu,%d,%.0f,%.0f,%.9g\n", backend, dataset, n, r, elapsed / iterations,
                gpu / iterations, sum);
  }
}
int main(int argc, char **argv) {
  try {
    size_t n = argc > 1 ? std::stoull(argv[1]) : 10000;
    int reps = argc > 2 ? std::stoi(argv[2]) : 20;
    std::string dataset = argc > 3 ? argv[3] : "mixed", selected = argc > 4 ? argv[4] : "all";
    if (argc > 5) {
      if (argc != 6 || std::string(argv[5]) != "--calibrated")
        throw std::invalid_argument("Unknown benchmark option");
      calibrated_measurements = true;
      const std::string cpu[] = {"reference", "aos",   "soa",   "neon",      "neon_cull",
                                 "pool1",     "pool2", "pool4", "pool_neon4"};
      if (std::find(std::begin(cpu), std::end(cpu), selected) == std::end(cpu))
        throw std::invalid_argument("Calibration requires one CPU kernel backend");
    }
    if (!n || n > 1000000 || reps < 1)
      throw std::runtime_error("Invalid size/repetitions");
    const std::string valid_backends[] = {"all",
                                          "reference",
                                          "aos",
                                          "soa",
                                          "neon",
                                          "neon_cull",
                                          "pool1",
                                          "pool2",
                                          "pool4",
                                          "pool_neon4",
                                          "gpu",
                                          "glb",
                                          "render",
                                          "spatial",
                                          "simulation",
                                          "simulation_reference",
                                          "simulation_fullstate",
                                          "simulation_access_old",
                                          "simulation_read_only",
                                          "simulation_pool1",
                                          "simulation_pool2",
                                          "simulation_pool4",
                                          "fixtures"};
    if (std::find(std::begin(valid_backends), std::end(valid_backends), selected) ==
        std::end(valid_backends))
      throw std::invalid_argument("Unknown backend: " + selected);
    bool simulation = selected.starts_with("simulation");
    bool dataset_ok =
        selected == "glb" ||
        (selected == "spatial"  ? (dataset == "uniform" || dataset == "clustered")
         : selected == "render" ? dataset == "offscreen"
         : simulation           ? dataset == "mixed"
                      : (dataset == "mixed" || dataset == "inside" || dataset == "outside"));
    if (!dataset_ok)
      throw std::invalid_argument("Unknown dataset for selected backend: " + dataset);
    std::puts("backend,dataset,n,repeat,wall_ns,gpu_ns,checksum");
    if (selected == "glb") {
      glb_benchmark(n, reps, dataset.c_str());
      return 0;
    }
    if (selected == "render") {
      render_benchmark(n, reps);
      return 0;
    }
    if (selected == "spatial") {
      bench::spatial(n, reps, dataset);
      return 0;
    }
    if (selected == "simulation" || selected == "simulation_reference" ||
        selected == "simulation_fullstate" || selected.starts_with("simulation_pool") ||
        selected == "simulation_access_old" || selected == "simulation_read_only") {
      Simulation sim(42, n);
      sim.profiling = true;
      sim.pose_only_sync = selected != "simulation_fullstate";
      if (selected == "simulation_access_old")
        sim.set_physics_access(false, true);
      if (selected == "simulation_read_only")
        sim.set_physics_access(true, true);
      if (selected.starts_with("simulation_pool"))
        sim.set_wind_workers(std::stoul(selected.substr(15)));
      if (selected == "simulation_reference")
        sim.set_force_evaluator(&Tornado::ReferenceForceAt);
      sim.set_wind_enabled(true);
      sim.set_tornado_direction({1, 0, 0});
      for (int i = 0; i < 60; ++i)
        sim.Update(1.f / 60);
      for (int i = 0; i < reps; ++i) {
        auto b = Clock::now();
        sim.Update(1.f / 60);
        auto e = Clock::now();
        size_t placed = 0;
        double checksum = 0;
        for (auto &o : sim.get_objects())
          if (!o->awaiting_spawn_ && !o->body_id_.IsInvalid()) {
            ++placed;
            checksum += o->position_.x + o->position_.y + o->position_.z;
          }
        std::printf("%s,placed_%zu_active_%u,%zu,%d,%.0f,0,%.17g\n", selected.c_str(), placed,
                    sim.active_body_count(), n, i,
                    std::chrono::duration<double, std::nano>(e - b).count(), checksum);
        const double phases[] = {sim.timings.wind_ns, sim.timings.physics_ns, sim.timings.sync_ns,
                                 sim.timings.spawn_ns};
        const char *names[] = {"wind", "physics", "sync", "spawn"};
        for (int k = 0; k < 4; ++k)
          std::printf("%s_%s,phases,%zu,%d,%.0f,0,0\n", selected.c_str(), names[k], n, i,
                      phases[k]);
      }
      return 0;
    }
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> d(-1, 1);
    std::vector<bench::Sample> a(n);
    for (auto &s : a) {
      s.p = {d(rng) * 35, 15 + d(rng) * 20, -8 + d(rng) * 35};
      s.v = {d(rng) * 20, d(rng) * 20, d(rng) * 20};
      if (dataset == "inside")
        s.p = {d(rng) * 3, 15 + d(rng) * 10, -8 + d(rng) * 3};
      if (dataset == "outside")
        s.p = {100 + d(rng) * 10, 15, -8 + d(rng) * 10};
    }
    // Include axis, height/radius boundaries, and tails in every correctness run.
    const simd_float3 edges[] = {{0, 0, -8},  {0, 30, -8}, {15, 0, -8},
                                 {0, -1, -8}, {0, 15, -8}, {45, 30, -8}};
    for (size_t i = 0; i < std::min(n, std::size(edges)); ++i)
      a[i].p = edges[i];
    if (selected == "fixtures") {
      std::puts("px,py,pz,vx,vy,vz,fx,fy,fz");
      Tornado t;
      for (auto s : a) {
        auto f = t.ReferenceForceAt(s.p, s.v);
        std::printf("%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n", s.p.x, s.p.y, s.p.z, s.v.x,
                    s.v.y, s.v.z, f.x, f.y, f.z);
      }
      return 0;
    }
    bench::SoA soa(a);
    Tornado t;
    std::vector<simd_float3> ref(n), out(n);
    bench::scalar(t, a, ref, true);
    auto run = [&](const char *name, auto fn) {
      if (selected != "all" && selected != name)
        return;
      fn();
      check(ref, out);
      measure(name, dataset.c_str(), n, fn, out, reps);
    };
    run("reference", [&] {
      bench::scalar(t, a, out, true);
      return 0.;
    });
    run("aos", [&] {
      bench::scalar(t, a, out);
      return 0.;
    });
    run("soa", [&] {
      bench::soa(t, soa, out);
      return 0.;
    });
    run("neon_cull", [&] {
      bench::neon(t, soa, out, 0, n, true);
      return 0.;
    });
    run("neon", [&] {
      bench::neon(t, soa, out);
      return 0.;
    });
    for (size_t workers : {1, 2, 4}) {
      bench::Pool pool(workers);
      auto name = "pool" + std::to_string(workers);
      run(name.c_str(), [&] {
        pool.run(n, [&](size_t b, size_t e) {
          bench::scalar(t, std::span(a).subspan(b, e - b), std::span(out).subspan(b, e - b));
        });
        return 0.;
      });
    }
    {
      bench::Pool pool(4);
      run("pool_neon4", [&] {
        pool.run(n, [&](size_t b, size_t e) { bench::neon(t, soa, out, b, e); });
        return 0.;
      });
    }
    if (selected == "all" || selected == "gpu") {
      GPU gpu(n);
      run("gpu", [&] { return gpu.run(a, out); });
    }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "ERROR: %s\n", e.what());
    return 1;
  }
}
