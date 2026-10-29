// Dedicated executable: counters never contaminate timing benchmark binaries.
#include "simulation/simulation.hpp"
#include <Jolt/Core/Memory.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <execinfo.h>
#include <new>
#include <stdexcept>
#include <string>
namespace {
std::atomic<bool> enabled{false};
std::atomic<size_t> cpp_calls{0}, cpp_bytes{0}, jolt_calls{0}, jolt_bytes{0};
void record(std::atomic<size_t> &calls, std::atomic<size_t> &bytes, size_t size) noexcept {
  if (enabled.load(std::memory_order_relaxed)) {
    calls.fetch_add(1, std::memory_order_relaxed);
    bytes.fetch_add(size, std::memory_order_relaxed);
  }
}
bool trace = false;
int trace_remaining = 2;
void trace_jolt() {
  if (trace && enabled && trace_remaining > 0) {
    --trace_remaining;
    void *frames[32];
    int n = backtrace(frames, 32);
    backtrace_symbols_fd(frames, n, 2);
  }
}
void reset() {
  cpp_calls = 0;
  cpp_bytes = 0;
  jolt_calls = 0;
  jolt_bytes = 0;
}
void *allocate(size_t n, size_t alignment = 0) {
  void *p = nullptr;
  if (alignment) {
    if (posix_memalign(&p, alignment, n ? n : 1))
      p = nullptr;
  } else
    p = std::malloc(n ? n : 1);
  if (!p)
    throw std::bad_alloc();
  record(cpp_calls, cpp_bytes, n);
  return p;
}
struct JoltHooks {
  JPH::AllocateFunction alloc = JPH::Allocate;
  JPH::ReallocateFunction realloc = JPH::Reallocate;
  JPH::AlignedAllocateFunction aligned = JPH::AlignedAllocate;
  static inline JoltHooks *current = nullptr;
  JoltHooks() {
    current = this;
    JPH::Allocate = [](size_t n) {
      auto p = current->alloc(n);
      if (p) {
        record(jolt_calls, jolt_bytes, n);
        trace_jolt();
      }
      return p;
    };
    JPH::Reallocate = [](void *p, size_t old, size_t n) {
      auto result = current->realloc(p, old, n);
      if (result)
        record(jolt_calls, jolt_bytes, n);
      return result;
    };
    JPH::AlignedAllocate = [](size_t n, size_t a) {
      auto p = current->aligned(n, a);
      if (p) {
        record(jolt_calls, jolt_bytes, n);
        trace_jolt();
      }
      return p;
    };
  }
  ~JoltHooks() {
    JPH::Allocate = alloc;
    JPH::Reallocate = realloc;
    JPH::AlignedAllocate = aligned;
    current = nullptr;
  }
};
} // namespace
void *operator new(size_t n) { return allocate(n); }
void *operator new[](size_t n) { return allocate(n); }
void *operator new(size_t n, std::align_val_t a) { return allocate(n, size_t(a)); }
void *operator new[](size_t n, std::align_val_t a) { return allocate(n, size_t(a)); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
void operator delete(void *p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void *p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void *p, size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t, std::align_val_t) noexcept { std::free(p); }
int main(int argc, char **argv) {
  try {
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
      JPH::RegisterDefaultAllocator();
      JoltHooks hooks;
      reset();
      enabled = true;
      auto a = ::operator new(37);
      auto b = ::operator new[](19);
      auto c = ::operator new(128, std::align_val_t(64));
      auto d = JPH::Allocate(41);
      auto e = JPH::AlignedAllocate(64, 32);
      d = JPH::Reallocate(d, 41, 83);
      enabled = false;
      bool ok = cpp_calls == 3 && cpp_bytes == 184 && jolt_calls == 3 && jolt_bytes == 188;
      ::operator delete(a);
      ::operator delete[](b);
      ::operator delete(c, std::align_val_t(64));
      JPH::Free(d);
      JPH::AlignedFree(e);
      if (!ok)
        throw std::runtime_error("Allocation counter self test failed");
      std::puts("PASS: C++ scalar/array/aligned and Jolt allocate/aligned/reallocate counters");
      return 0;
    }
    size_t n = argc > 1 ? std::stoul(argv[1]) : 600, workers = argc > 2 ? std::stoul(argv[2]) : 0;
    trace = std::getenv("QUACK_ALLOC_TRACE") != nullptr;
    Simulation sim(42, n);
    sim.set_wind_workers(workers);
    sim.set_wind_enabled(true);
    sim.set_tornado_direction({1, 0, 0});
    for (int i = 0; i < 60; ++i)
      sim.Update(1.f / 60);
    JoltHooks hooks;
    std::puts(
        "n,workers,step,cpp_calls,cpp_requested_bytes,jolt_calls,jolt_requested_bytes,active");
    size_t total_cpp = 0;
    for (int step = 0; step < 600; ++step) {
      reset();
      enabled = true;
      sim.Update(1.f / 60);
      enabled = false;
      total_cpp += cpp_calls.load();
      std::printf("%zu,%zu,%d,%zu,%zu,%zu,%zu,%u\n", n, workers, step, cpp_calls.load(),
                  cpp_bytes.load(), jolt_calls.load(), jolt_bytes.load(), sim.active_body_count());
    }
    if (argc > 3 && std::string(argv[3]) == "--require-no-cpp" && total_cpp)
      throw std::runtime_error("Unexpected steady-state C++ allocation");
  } catch (const std::exception &e) {
    enabled = false;
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
