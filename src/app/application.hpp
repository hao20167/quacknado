#pragma once

#include <memory>

#include <SDL3/SDL.h>

#include "renderer/metal_renderer.hpp"
#include "simulation/simulation.hpp"


namespace quacknado {

class Application {
private:
  bool sdl_initialized_ = false;
  bool running_ = false;
  SDL_Window* window_ = nullptr;
  std::unique_ptr<MetalRenderer> renderer_;
  std::unique_ptr<Simulation> simulation_;

public:
  Application() = default;
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  [[nodiscard]]
  bool Initialize();

  int Run();

private:
  void ProcessEvents();
};

} // namespace quacknado
