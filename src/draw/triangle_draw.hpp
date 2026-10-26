#pragma once 

#include "renderer/render_module.hpp"

struct alignas(8) TriangleOffset {
  float x = 0.5f;
  float y = 0.0f;
};

static_assert(sizeof(TriangleOffset) == 8);
static_assert(alignof(TriangleOffset) == 8);

class TriangleDraw final : public IRenderModule {
private:
  TriangleOffset offset_ {};
  NS::SharedPtr<MTL::RenderPipelineState> pipeline_;
public:
  TriangleDraw() = default;
  ~TriangleDraw() override;

  bool Initialize(MTL::Device* device, MTL::Library* library, MTL::PixelFormat color_format) override;
  void Record(MTL::RenderCommandEncoder* encoder, int width, int height) override;
};
