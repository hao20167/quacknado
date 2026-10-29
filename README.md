# Quacknado

A native Apple Silicon C++20 tornado sandbox with Jolt rigid-body physics, SDL3,
Metal rendering, GLB instancing, and reproducible performance experiments.

The interactive scene and replay request 10000 dynamic objects by default across several model types plus
scene props. Placement may defer objects when no space is available. Arrow keys
move the tornado; closing the window exits. Collision shapes and visual scale are
configured independently in `src/simulation/objs/obj_module.hpp`.
`QUACK_SCENE_COUNT=1..16381` overrides the requested population (three body slots are
reserved for props). For 10k: `QUACK_SCENE_COUNT=10000 ./build/default/quacknado`.
Scenes above 1000 use collision-free layered placement and immediate large-scene
respawn; 10,000 requested objects are present from initialization. See the
[10k spawn verification](docs/benchmarks/spawn-10k/report.md) for actual body and
completed-draw submission counts, including the distinction from visible pixels.

## Build and run

Requires macOS/Apple Silicon, Xcode with the Metal compiler, CMake 4.4+, Ninja,
and Python 3.10+. Tested with Apple M1 Pro, macOS 14.7.5 and Apple Clang 16.
Initialize the vendored submodules if starting from a fresh checkout:

```sh
git submodule update --init --recursive
cmake --preset default
cmake --build --preset default -j 8
ctest --preset default
./build/default/quacknado
```

The default preset explicitly builds Release and keeps the existing executable path.
Startup prints build config, scene count and physics worker count. An empty single-config
build type also defaults to Release; explicit Debug/check builds stay Debug.
The former empty default compiled both Jolt and the game without optimization and
caused large collision/catch-up stalls. See [the breakthrough investigation](docs/benchmarks/interactions/breakthrough/story.md).

Run commands from the repository root. The full build copies models and shaders
next to the game executable. Benchmarks that load assets directly require the
repository root as their working directory.

## What the measurements establish

| Evidence | Result on the development machine | Scope |
|---|---|---|
| Dense contacts and real-clock replay, 1000 bodies | Dense physics median 44.39 → 1.87 ms; real-clock frame p99 137.47 → 20.65 ms | Fixes the previously unoptimized default build; 373/900 → 1/900 frames above 33 ms on the measured fixture |
| 10k no-wind collision pile | Physics median 20.52 → 11.23 ms; real-clock frame p99 185.62 → 14.54 ms | Scene-aware worker policy; >33 ms frames 1413 → 20/1800, rare hitches retained in the report |
| Calibrated wind kernel, 1M mixed samples | NEON + 4 workers about 6.65× over reference | Sample computation, not 1M rigid bodies |
| Actual windowed game | CPU submission about 0.266 → 0.084 ms | Frame median remained about 8.33 ms under presentation pacing |
| Integrated wind worker pool | Did not improve the measured small-scene pipeline | Direct path remains the game default |
| Memory probe | No intercepted C++ new calls in measured trajectories; Jolt still allocates | Does not establish zero allocations for the whole process |
| Renderer correctness | Mixed GLB pixel comparisons and fail-fast GPU sanitizer checks | Three-model regression fixture, not every possible scene |

See [the full report](docs/benchmarks/report.md) for raw-data locations, before/after
experiments, numerical tolerance, negative results and limitations. Historical
measurements identify their source versions; newer code is not assumed to reproduce
old timings exactly. This project demonstrates C++ ownership/templates/STL, SIMD,
concurrency, data structures and Python analysis. It does not currently include a
Python native binding, networking or an agentic LLM runtime.

## Reproduce an experiment

```sh
# Historical runners below use the perf build; their 600-object fixtures are explicit.
cmake --preset perf
cmake --build --preset perf -j 8

# Independent Python reference against 10,003 C++ force fixtures.
python3 scripts/check_reference.py

# CPU benchmark with correctness check and calibrated batches.
./build/perf/Release/quack_bench 10003 12 mixed neon --calibrated

# GPU pixel/resource-lifetime regression; requires a desktop Metal device.
MTL_DEBUG_LAYER=1 ./build/perf/Release/render_regression build/perf/Release/object.metallib

# Actual windowed replay; opens the game and exits after 210 frames.
QUACK_REPLAY_FRAMES=210 QUACK_REPLAY_MODE=instanced ./build/perf/Release/quacknado

# New campaign paths preserve recorded evidence. Choose unused directories.
python3 scripts/benchmark_calibrated.py --output /tmp/quack-cpu-campaign --no-report
python3 scripts/benchmark_windowed.py --output /tmp/quack-window-campaign
```

`quack_bench` arguments are `N repetitions dataset backend [--calibrated]`.
Calibration is supported for CPU kernels only. Backend `gpu` measures wind compute;
`glb` measures the actual GLB renderer (`dataset` is a model name such as `chicken`).
Campaign runners refuse to overwrite metadata/raw/summary in an existing output.
They assume one writer per output directory; use distinct directories for parallel
runs. They may update their numbered report section after a successful campaign.

## Validation and build structure

```sh
cmake --preset check
cmake --build --preset check -j 8
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 ctest --preset check
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 MTL_DEBUG_LAYER=1 \
  ./build/check/Debug/render_regression build/check/Debug/object.metallib
```

CPU tests are registered by default. GPU regression is run explicitly, or registered
with `-DQUACK_GPU_TESTS=ON` when configuring a machine with GPU access. The allocation
probe deliberately uses an uninstrumented simulation variant; Jolt/vendor code is
not claimed as sanitizer-covered. A separate TSan pool command is in the report.

`quack_sim` and `quack_render` are shared by the game and validation targets. CMake
source lists, shader/assets and test registration are in `cmake/`. Scalar reference,
SoA, NEON, worker-pool and GPU experiments retain comparable baselines. The game uses
pose-only sync, coalesced Jolt state reads and bounded GLB instancing batches.
Jolt chooses background workers by requested scene size: 1 below 4096, 2 below 8192,
and 6 from 8192 objects, capped at hardware threads minus two; fewer than four
reported hardware threads use serial. Explicit `QUACK_PHYSICS_WORKERS` overrides this policy.
The [10k no-wind collision investigation](docs/benchmarks/collision-pile/story.md)
explains why the earlier small-scene worker policy caused large piles to lag. In the earlier five 900-frame replays at 600 objects,
one worker reduced median simulation CPU by about 41%; simulation p99 rose about 4%
and whole-frame p99 was nearly unchanged. This is a measured trade-off, not an FPS claim.
`QUACK_PHYSICS_WORKERS=0` selects serial; counts 0..16 are supported.
`QUACK_PHYSICS_QOS=1` optionally requests macOS USER_INITIATED for workers
(requires a nonzero worker count); QoS is off by default because it did not resolve tail.
This is separate from the wind worker pool, which remains off by default. See
[the interaction optimization story](docs/benchmarks/interactions/story.md) for
profiling, crossover, headless and windowed A/B results, including tail-time risks.
The [latest 1000-object investigation](docs/benchmarks/interactions/breakthrough/story.md)
measures the actual default build and real-time catch-up; `QUACK_REPLAY_CLOCK=realtime`
shares the interactive fixed-step policy. The default replay clock remains fixed
(one tick per frame).

The GLB loader supports the supplied static triangle assets, node transforms,
embedded PNG/JPEG textures and alpha masks. Animation, skinning, PBR lighting,
blended materials, external images and glTF extensions are outside its current scope.
