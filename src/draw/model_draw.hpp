#pragma once

#include <memory>
#include <string>
#include "renderer/render_module.hpp"
#include "simulation/objs/obj_module.hpp"

class ModelDraw final : public IRenderModule {
  struct Asset;
  std::shared_ptr<Asset> asset_;
  NS::SharedPtr<MTL::RenderPipelineState> pipeline_;
  IObjModule* object_;
  std::string path_;
public:
  ModelDraw(IObjModule* object, std::string path);
  ~ModelDraw() override;
  bool Initialize(MTL::Device*, MTL::Library*, MTL::PixelFormat) override;
  void Record(MTL::RenderCommandEncoder*, int width, int height) override;
};
