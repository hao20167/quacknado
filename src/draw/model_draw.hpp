#pragma once
#include "core/metal_ptr.hpp"

#include "renderer/render_module.hpp"
#include "simulation/objs/obj_module.hpp"
#include <memory>
#include <string>

class ModelDraw final : public IRenderModule {
  struct Asset;
  std::shared_ptr<Asset> asset_;
  quacknado::metal::Ptr<MTL::RenderPipelineState> pipeline_;
  std::vector<IObjModule *> objects_;
  std::string path_;

public:
  ModelDraw(IObjModule *object, std::string path);
  void AddObject(IObjModule *object) { objects_.push_back(object); }
  bool instancing = true;
  size_t last_draw_calls = 0;
  ModelSubmissions last_models;
  ModelSubmissions SubmittedModels() const override { return last_models; }
  ~ModelDraw() override;
  bool Initialize(MTL::Device *, MTL::Library *, MTL::PixelFormat) override;
  void Record(MTL::RenderCommandEncoder *, int width, int height) override;
};
