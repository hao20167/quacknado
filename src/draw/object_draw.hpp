#pragma once 

#include <SDL3/SDL.h>

#include "renderer/render_module.hpp"
#include "simulation/objs/obj_module.hpp"

class ObjectDraw final : public IRenderModule {
private:
  NS::SharedPtr<MTL::RenderPipelineState> pipeline_;
  SDL_Window* window_;
  UInt64 start_;
  NS::SharedPtr<MTL::Buffer> buffer_;
  IObjModule* obj_;
public:
  ObjectDraw() = default;
  explicit ObjectDraw(IObjModule* ptr);
  ~ObjectDraw() override;

  bool Initialize(MTL::Device* device, MTL::Library* library, MTL::PixelFormat color_format) override;
  void Record(MTL::RenderCommandEncoder* encoder, int width, int height) override;
  void setObject(IObjModule* ptr) {
    obj_ = ptr;
  }
};
