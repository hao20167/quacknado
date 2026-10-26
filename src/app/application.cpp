#include "app/application.hpp"

#include <cassert>

#include <SDL3/SDL.h>

#include "renderer/metal_renderer.hpp"
#include "draw/duck_draw.hpp"

namespace quacknado {

constexpr double FIX_DT = 1.0 / 60.0;
constexpr Uint64 TARGET_FRAME_NS = 1000000000ULL / 60;

Uint64 previous_time {};

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
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }
  sdl_initialized_ = true;

  window_ = SDL_CreateWindow(
    "Quacknado", 
    1280, 
    720,
    SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
  );
  if (window_ == nullptr) {
    SDL_Log("SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }

  simulation_ = std::make_unique<Simulation>();
  renderer_ = std::make_unique<MetalRenderer>();

  renderer_->AddModule(std::make_unique<DuckDraw>(simulation_->duck_.get()));

  const char* base_path = SDL_GetBasePath(); 
  if (base_path == nullptr) {
    SDL_Log("SDL_GetBasePath failed.\n");
    return false;
  }
  std::string metallib_path = std::string(base_path) + "duck.metallib";

  if (!renderer_->Initialize(window_, metallib_path)) {
    return false;
  }

  running_ = true;
  previous_time = SDL_GetTicksNS();

  return true;
}

void Application::ProcessEvents() {
  SDL_Event event {};
  while (SDL_PollEvent(&event)) {
    const auto& type = event.type;
    if (type == SDL_EVENT_QUIT || type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
      running_ = false;
    } else if (type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || type == SDL_EVENT_WINDOW_METAL_VIEW_RESIZED) {
      renderer_->Resize();
    }
  }
}

int Application::Run() {
  // WARN: resolve case where race condition happens in parallel running
  double accumulator {};
  while (running_) { 
    ProcessEvents();
    if (!running_) {
      break;
    }

    Uint64 current_time = SDL_GetTicksNS();
    double frame_time = static_cast<double>(current_time - previous_time) / 1000000000.0;
    previous_time = current_time;
    accumulator += std::min(frame_time, 0.1); // WARN: this might make gametime slower than realtime

    while (accumulator >= FIX_DT) {
      simulation_->update(static_cast<float>(FIX_DT));
      accumulator -= FIX_DT;
    }

    renderer_->Render();
  }

  return 0;
}

}
