#pragma once 

#include <SDL3/SDL.h>

#include "renderer/render_module.hpp"

class CubeDraw final : public IRenderModule {
private:
  NS::SharedPtr<MTL::RenderPipelineState> pipeline_;
  SDL_Window* window_;
  UInt64 start_;
  NS::SharedPtr<MTL::Buffer> buffer_;
public:
  CubeDraw() = default;
  ~CubeDraw() override;

  bool Initialize(MTL::Device* device, MTL::Library* library, MTL::PixelFormat color_format) override;
  void Record(MTL::RenderCommandEncoder* encoder, int width, int height) override;
};
