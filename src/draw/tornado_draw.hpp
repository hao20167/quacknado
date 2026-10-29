#pragma once
#include "core/metal_ptr.hpp"

#include "renderer/render_module.hpp"
#include "simulation/env/tornado.hpp"

class TornadoDraw final : public IRenderModule {
  const Tornado* tornado_;
  quacknado::metal::Ptr<MTL::RenderPipelineState> pipeline_;
  quacknado::metal::Ptr<MTL::DepthStencilState> depth_state_;
public:
  explicit TornadoDraw(const Tornado* tornado) : tornado_(tornado) {}
  bool Initialize(MTL::Device*, MTL::Library*, MTL::PixelFormat) override;
  void Record(MTL::RenderCommandEncoder*, int width, int height) override;
};
