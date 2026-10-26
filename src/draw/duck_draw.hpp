#pragma once 

#include <SDL3/SDL.h>

#include "renderer/render_module.hpp"
#include "simulation/objs/duck.hpp"

class DuckDraw final : public IRenderModule {
private:
  NS::SharedPtr<MTL::RenderPipelineState> pipeline_;
  SDL_Window* window_;
  UInt64 start_;
  NS::SharedPtr<MTL::Buffer> buffer_;
  Duck* duck_;
public:
  DuckDraw() = default;
  DuckDraw(Duck* ptr);
  ~DuckDraw() override;

  bool Initialize(MTL::Device* device, MTL::Library* library, MTL::PixelFormat color_format) override;
  void Record(MTL::RenderCommandEncoder* encoder, int width, int height) override;
  void setObject(Duck* ptr) {
    duck_ = ptr;
  }
};
