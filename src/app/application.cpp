#include "application.hpp"
#include "core/fixed_step.hpp"
#include "simulation/physics/worker_policy.hpp"

#include <cassert>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>

#include "draw/model_draw.hpp"
#include "draw/object_draw.hpp"
#include "draw/tornado_draw.hpp"
#include "renderer/metal_renderer.hpp"

namespace quacknado {

Uint64 previous_time{};

Application::~Application() {
  renderer_.reset();
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  if (sdl_initialized_) {
    SDL_Quit();
    sdl_initialized_ = false;
  }
}

bool Application::Initialize() {
  if (const char *value = std::getenv("QUACK_SCENE_COUNT")) {
    const std::string text = value;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), scene_count_);
    if (error != std::errc{} || end != text.data() + text.size() || scene_count_ < 1 ||
        scene_count_ > kMaxSceneObjects) {
      SDL_Log("Invalid QUACK_SCENE_COUNT (1..%zu)", kMaxSceneObjects);
      return false;
    }
  }
  physics_workers_ = DefaultPhysicsWorkers(scene_count_, std::thread::hardware_concurrency());
  if (const char *value = std::getenv("QUACK_PHYSICS_WORKERS")) {
    std::string text = value;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), physics_workers_);
    if (error != std::errc{} || end != text.data() + text.size() || physics_workers_ < 0 ||
        physics_workers_ > 16) {
      SDL_Log("Invalid QUACK_PHYSICS_WORKERS (0..16)");
      return false;
    }
  }
  if (const char *value = std::getenv("QUACK_PHYSICS_QOS")) {
    const std::string text = value;
    if ((text != "0" && text != "1") || (text == "1" && !physics_workers_)) {
      SDL_Log("Invalid QUACK_PHYSICS_QOS (0/1; 1 requires workers)");
      return false;
    }
    physics_worker_qos_ = text == "1";
  }
  if (const char *value = std::getenv("QUACK_REPLAY_FRAMES")) {
    std::string text = value;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), replay_frames_);
    if (error != std::errc{} || end != text.data() + text.size() || !replay_frames_ ||
        replay_frames_ > 2000) {
      SDL_Log("Invalid QUACK_REPLAY_FRAMES (1..2000)");
      return false;
    }
    if (const char *clock = std::getenv("QUACK_REPLAY_CLOCK")) {
      const std::string text = clock;
      if (text != "fixed" && text != "realtime") {
        SDL_Log("Invalid QUACK_REPLAY_CLOCK (fixed/realtime)");
        return false;
      }
      replay_realtime_ = text == "realtime";
    }
    if (const char *wind = std::getenv("QUACK_REPLAY_WIND")) {
      const std::string text = wind;
      if (text != "0" && text != "1") {
        SDL_Log("Invalid QUACK_REPLAY_WIND (0/1)");
        return false;
      }
      replay_wind_ = text == "1";
    }
    if (const char *mode = std::getenv("QUACK_REPLAY_MODE")) {
      if (std::string(mode) != "individual" && std::string(mode) != "instanced") {
        SDL_Log("Invalid replay mode");
        return false;
      }
      replay_instancing_ = std::string(mode) == "instanced";
    }
  }
  SDL_Log("Build=%s scene=%zu physics_workers=%d", QUACK_BUILD_TYPE, scene_count_,
          physics_workers_);
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }
  sdl_initialized_ = true;

  window_ = SDL_CreateWindow("Quacknado", 1280, 720,
                             SDL_WINDOW_METAL | (replay_frames_ ? 0 : SDL_WINDOW_RESIZABLE) |
                                 SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (window_ == nullptr) {
    SDL_Log("SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }

  simulation_ = replay_frames_
                    ? std::make_unique<Simulation>(
                          42, scene_count_,
                          PhysicsOptions{physics_workers_, true, false, physics_worker_qos_})
                    : std::make_unique<Simulation>(
                          std::random_device{}(), scene_count_,
                          PhysicsOptions{physics_workers_, true, false, physics_worker_qos_});
  simulation_->set_wind_enabled(true);
  renderer_ = std::make_unique<MetalRenderer>();

  const char *base_path = SDL_GetBasePath();
  if (base_path == nullptr) {
    SDL_Log("SDL_GetBasePath failed.\n");
    return false;
  }
  std::string metallib_path = std::string(base_path) + "object.metallib";

  std::map<std::string, ModelDraw *> model_groups;
  for (const auto &object : simulation_->get_objects()) {
    if (object->name_.empty()) {
      renderer_->AddModule(std::make_unique<ObjectDraw>(object.get()));
    } else {
      auto &group = model_groups[object->name_];
      if (group && (!replay_frames_ || replay_instancing_))
        group->AddObject(object.get());
      else {
        auto module = std::make_unique<ModelDraw>(
            object.get(), std::string(base_path) + "assets/models/" + object->name_ + ".glb");
        module->instancing = (!replay_frames_ || replay_instancing_);
        group = module.get();
        renderer_->AddModule(std::move(module));
      }
    }
  }

  renderer_->AddModule(std::make_unique<TornadoDraw>(&simulation_->get_tornado()));

  if (!renderer_->Initialize(window_, metallib_path)) {
    return false;
  }

  running_ = true;
  previous_time = SDL_GetTicksNS();

  return true;
}

void Application::ProcessEvents() {
  SDL_Event event{};
  while (SDL_PollEvent(&event)) {
    const auto &type = event.type;
    if (type == SDL_EVENT_QUIT || type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
      running_ = false;
    } else if (type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
               type == SDL_EVENT_WINDOW_METAL_VIEW_RESIZED) {
      renderer_->Resize();
    }
  }
}

int Application::Run() {
  if (replay_frames_)
    return RunReplay();
  FixedStep stepper;
  while (running_) {
    ProcessEvents();
    if (!running_) {
      break;
    }

    simd_float3 direction{0, 0, 0};
    if (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS) {
      const bool *keys = SDL_GetKeyboardState(nullptr);
      direction = {float(keys[SDL_SCANCODE_RIGHT]) - float(keys[SDL_SCANCODE_LEFT]), 0.0f,
                   float(keys[SDL_SCANCODE_UP]) - float(keys[SDL_SCANCODE_DOWN])};
    }
    simulation_->set_tornado_direction(direction);

    Uint64 current_time = SDL_GetTicksNS();
    double frame_time = static_cast<double>(current_time - previous_time) / 1000000000.0;
    previous_time = current_time;
    stepper.Advance(frame_time, [&](float dt) { simulation_->Update(dt); });

    renderer_->Render();
  }

  return 0;
}

} // namespace quacknado

namespace quacknado {
int Application::RunReplay() {
  struct Row {
    double simulation, physics, wind, submission, render, frame, elapsed, pending, checksum;
    unsigned steps, active;
    size_t total_steps;
    size_t placed, pending_count, model_objects, model_instances, respawned;
    int width, height;
  };
  std::vector<Row> rows;
  rows.reserve(replay_frames_);
  simulation_->profiling = true;
  simulation_->set_wind_enabled(replay_wind_);
  std::map<std::string, size_t> census;
  for (const auto &object : simulation_->get_objects())
    if (!object->name_.empty() && !object->awaiting_spawn_ && !object->body_id_.IsInvalid())
      ++census[object->name_];
  for (const auto &[name, count] : census)
    SDL_Log("ReplayModel name=%s objects=%zu", name.c_str(), count);
  renderer_->StartProfile(replay_frames_);
  using Clock = std::chrono::steady_clock;
  auto previous = Clock::now();
  FixedStep stepper;
  size_t total_steps = 0;
  for (size_t frame = 0; frame < replay_frames_; ++frame) {
    auto start = Clock::now();
    const double elapsed = std::chrono::duration<double>(start - previous).count();
    previous = start;
    ProcessEvents();
    if (!running_) {
      SDL_Log("Replay interrupted");
      return 1;
    }
    double physics = 0, wind = 0;
    size_t respawned = 0;
    auto update = [&](float dt) {
      simulation_->set_tornado_direction({(total_steps / 60) % 2 ? -1.f : 1.f, 0, 0});
      simulation_->Update(dt);
      respawned += simulation_->respawned_last_update();
      physics += simulation_->timings.physics_ns;
      wind += simulation_->timings.wind_ns;
      ++total_steps;
    };
    auto before = Clock::now();
    unsigned steps;
    if (replay_realtime_)
      steps = stepper.Advance(elapsed, update);
    else {
      update(static_cast<float>(FixedStep::dt));
      steps = 1;
    }
    auto after = Clock::now();
    renderer_->Render();
    auto end = Clock::now();
    if (!renderer_->last_submitted) {
      SDL_Log("Replay drawable/command unavailable");
      return 1;
    }
    double checksum = 0;
    size_t placed = 0;
    for (const auto &o : simulation_->get_objects())
      if (!o->awaiting_spawn_ && !o->body_id_.IsInvalid()) {
        checksum += o->position_.x + o->position_.y + o->position_.z;
        ++placed;
      }
    auto ns = [](auto d) { return std::chrono::duration<double, std::nano>(d).count(); };
    int w, h;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    rows.push_back({ns(after - before), physics, wind, renderer_->last_submission_ns,
                    ns(end - after), ns(end - start), elapsed, stepper.PendingSeconds(), checksum,
                    steps, simulation_->active_body_count(), total_steps, placed,
                    simulation_->get_objects().size() - placed, renderer_->last_models.objects,
                    renderer_->last_models.instances, respawned, w, h});
  }
  const auto &gpu = renderer_->FinishProfile();
  if (gpu.size() != rows.size())
    return 1;
  std::puts(
      "frame,mode,physics_workers,physics_qos,scene_count,clock,wind_enabled,steps,total_steps,"
      "elapsed_s,pending_s,simulation_ns,physics_ns,wind_ns,submission_ns,render_call_ns,"
      "cpu_frame_ns,gpu_command_ns,checksum,active,width,height,placed,pending,model_objects,"
      "model_instance_invocations,respawned");
  for (size_t i = 0; i < rows.size(); ++i) {
    const auto &r = rows[i];
    std::printf("%zu,%s,%d,%d,%zu,%s,%d,%u,%zu,%.9g,%.9g,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,"
                "%.0f,%.17g,%u,%d,%d,%zu,%zu,%zu,%zu,%zu\n",
                i, replay_instancing_ ? "instanced" : "individual", physics_workers_,
                int(physics_worker_qos_), scene_count_, replay_realtime_ ? "realtime" : "fixed",
                int(replay_wind_), r.steps, r.total_steps, r.elapsed, r.pending, r.simulation,
                r.physics, r.wind, r.submission, r.render, r.frame, gpu[i], r.checksum, r.active,
                r.width, r.height, r.placed, r.pending_count, r.model_objects, r.model_instances,
                r.respawned);
  }
  return 0;
}
} // namespace quacknado
