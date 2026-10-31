#pragma once
#include "core/metal_ptr.hpp"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <SDL3/SDL.h>
#include <deque>
#include <vector>

#include "core/extent.hpp"
#include "renderer/render_module.hpp"

namespace quacknado {

class MetalRenderer {
private:
  std::vector<std::unique_ptr<IRenderModule>> modules_;
  SDL_Window *window_ = nullptr;
  SDL_MetalView metal_view_ = nullptr;
  CA::MetalLayer *metal_layer_ = nullptr;
  quacknado::metal::Ptr<MTL::Device> device_;
  quacknado::metal::Ptr<MTL::CommandQueue> command_queue_;
  Extent2D drawable_extent_{};
  quacknado::metal::Ptr<MTL::DepthStencilState> depth_state_;
  quacknado::metal::Ptr<MTL::Texture> depth_texture_;

  std::deque<metal::Ptr<MTL::CommandBuffer>> pending_profile_;
  std::vector<double> gpu_ns_;
  void RetireProfileFrame();

public:
  bool profiling = false;
  bool last_submitted = false;
  IRenderModule::ModelSubmissions last_models;
  double last_submission_ns = 0;
  const std::vector<double> &FinishProfile();
  void StartProfile(size_t frames) {
    FinishProfile();
    gpu_ns_.clear();
    gpu_ns_.reserve(frames);
    profiling = true;
  }
  MetalRenderer() = default;
  ~MetalRenderer();

  MetalRenderer(const MetalRenderer &) = delete;
  MetalRenderer &operator=(const MetalRenderer &) = delete;

  void AddModule(std::unique_ptr<IRenderModule> module);
  [[nodiscard]] bool Initialize(SDL_Window *window, const std::string &metallib_path);
  void Resize();
  void Render();
};

} // namespace quacknado
