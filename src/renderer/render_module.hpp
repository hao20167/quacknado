#pragma once

#include <Metal/Metal.hpp>

class IRenderModule {
public:
  struct ModelSubmissions {
    size_t objects = 0, instances = 0;
  };
  virtual ModelSubmissions SubmittedModels() const { return {}; }
  virtual ~IRenderModule() = default;

  virtual bool Initialize(MTL::Device *device, MTL::Library *library,
                          MTL::PixelFormat color_format) = 0;
  virtual void Record(MTL::RenderCommandEncoder *encoder, int width, int height) = 0;
};
