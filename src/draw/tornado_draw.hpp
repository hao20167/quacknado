#pragma once

#include "renderer/render_module.hpp"
#include "simulation/env/tornado.hpp"

class TornadoDraw final : public IRenderModule {
  const Tornado* tornado_;
  NS::SharedPtr<MTL::RenderPipelineState> pipeline_;
  NS::SharedPtr<MTL::DepthStencilState> depth_state_;
public:
  explicit TornadoDraw(const Tornado* tornado) : tornado_(tornado) {}
  bool Initialize(MTL::Device*, MTL::Library*, MTL::PixelFormat) override;
  void Record(MTL::RenderCommandEncoder*, int width, int height) override;
};
