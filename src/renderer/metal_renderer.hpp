#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <SDL3/SDL.h>

#include "core/extent.hpp"

namespace quacknado {

class MetalRenderer {
private:
  SDL_Window* window_;
  SDL_MetalView metal_view_;
  CA::MetalLayer* metal_layer_;
  NS::SharedPtr<MTL::Device> device_;
  NS::SharedPtr<MTL::CommandQueue> command_queue_;
  Extent2D drawable_extent_ {};

public:
  MetalRenderer() = default;
  ~MetalRenderer();

  MetalRenderer(const MetalRenderer&) = delete;
  MetalRenderer& operator=(const MetalRenderer&) = delete;

  [[nodiscard]]
  bool Initialize(SDL_Window* window);
  void Resize();
  void Render();
};

}
