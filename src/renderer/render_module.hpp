#pragma once

#include <Metal/Metal.hpp>

class IRenderModule {
public:
  virtual ~IRenderModule() = default;

  virtual bool Initialize(MTL::Device* device, MTL::Library* library, MTL::PixelFormat color_format) = 0;
  virtual void Record(MTL::RenderCommandEncoder* encoder, int width, int height) = 0;
};
