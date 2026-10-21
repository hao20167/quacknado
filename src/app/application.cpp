#include <SDL3/SDL.h>

#include "app/application.hpp"
#include "SDL3/SDL_events.h"
#include "renderer/metal_renderer.hpp"
#include "draw/triangle_draw.hpp"

namespace quacknado {

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

  renderer_ = std::make_unique<MetalRenderer>();

  renderer_->AddModule(std::make_unique<TriangleDraw>());

  const char* base_path = SDL_GetBasePath(); 
  if (base_path == nullptr) {
    SDL_Log("SDL_GetBasePath failed.\n");
    return false;
  }
  std::string metallib_path = std::string(base_path) + "triangle.metallib";

  if (!renderer_->Initialize(window_, metallib_path)) {
    return false;
  }

  running_ = true;

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
  while (running_) { 
    ProcessEvents();
    if (!running_) {
      break;
    }
    renderer_->Render();
  }

  return 0;
}

}
