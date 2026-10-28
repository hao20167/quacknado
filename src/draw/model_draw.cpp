#include "draw/model_draw.hpp"
#include "assets/glb_asset.hpp"

#include <SDL3/SDL.h>
#include <map>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace {
struct Uniforms {
  simd_float4 orientation;
  simd_float4 offset_aspect;
  simd_float4 scale;
};
struct MaterialUniforms {
  simd_float4 base_color;
  simd_float4 alpha;
};
}
struct ModelDraw::Asset {
  struct Primitive {
    NS::SharedPtr<MTL::Buffer> vertices, indices;
    NS::SharedPtr<MTL::Texture> texture;
    size_t index_count;
    MaterialUniforms material;
  };
  std::vector<Primitive> primitives;
};

ModelDraw::ModelDraw(IObjModule* object, std::string path)
    : object_(object), path_(std::move(path)) {}
ModelDraw::~ModelDraw() = default;

bool ModelDraw::Initialize(MTL::Device* device, MTL::Library* library, MTL::PixelFormat format) {
  if (!device || !library || pipeline_.get()) return false;
  try {
    using Key = std::pair<MTL::Device*, std::string>;
    static std::map<Key, std::weak_ptr<Asset>> cache;
    static std::map<std::tuple<MTL::Device*, MTL::Library*, MTL::PixelFormat>, NS::SharedPtr<MTL::RenderPipelineState>> pipelines;
    const auto pipeline_key = std::make_tuple(device, library, format);
    auto& pipeline = pipelines[pipeline_key];
    if (!pipeline.get()) {
      auto vertex = NS::TransferPtr(library->newFunction(NS::String::string("vertex_model", NS::UTF8StringEncoding)));
      auto fragment = NS::TransferPtr(library->newFunction(NS::String::string("fragment_model", NS::UTF8StringEncoding)));
      if (!vertex.get() || !fragment.get()) throw std::runtime_error("Model shader functions missing");
      auto descriptor = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
      descriptor->setVertexFunction(vertex.get());
      descriptor->setFragmentFunction(fragment.get());
      descriptor->colorAttachments()->object(0)->setPixelFormat(format);
      descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
      NS::Error* error = nullptr;
      pipeline = NS::TransferPtr(device->newRenderPipelineState(descriptor.get(), &error));
      if (!pipeline.get()) throw std::runtime_error(error ? error->localizedDescription()->utf8String() : "Model pipeline failed");
    }
    pipeline_ = pipeline;
    auto& cached = cache[{device, path_}];
    asset_ = cached.lock();
    if (!asset_) {
      auto loaded = quacknado::LoadGlb(path_, device);
      asset_ = std::make_shared<Asset>();
      for (auto& p : loaded.primitives) {
        Asset::Primitive primitive;
        primitive.vertices = NS::TransferPtr(device->newBuffer(p.vertices.data(), p.vertices.size() * sizeof(quacknado::ModelVertex), MTL::ResourceStorageModeShared));
        primitive.indices = NS::TransferPtr(device->newBuffer(p.indices.data(), p.indices.size() * sizeof(uint32_t), MTL::ResourceStorageModeShared));
        if (!primitive.vertices.get() || !primitive.indices.get()) throw std::runtime_error("Model buffer allocation failed");
        primitive.texture = std::move(p.texture);
        primitive.index_count = p.indices.size();
        primitive.material = {p.base_color, {p.alpha_cutoff, 0, 0, 0}};
        asset_->primitives.push_back(std::move(primitive));
      }
      cached = asset_;
      SDL_Log("Loaded %s (%zu primitives)", path_.c_str(), asset_->primitives.size());
    }
    return true;
  } catch (const std::exception& error) {
    SDL_Log("Model load failed [%s]: %s", path_.c_str(), error.what());
    return false;
  }
}

void ModelDraw::Record(MTL::RenderCommandEncoder* encoder, int width, int height) {
  if (!encoder || !asset_ || !object_ || height <= 0 ||
      object_->awaiting_spawn_ || object_->body_id_.IsInvalid()) return;
  const float scale = object_->visual_height_ * 0.5f;
  Uniforms uniforms {object_->orientation_.vector,
      {object_->position_.x, object_->position_.y, object_->position_.z, float(width) / height},
      {scale, scale, scale, 0}};
  encoder->setRenderPipelineState(pipeline_.get());
  encoder->setCullMode(MTL::CullModeNone);
  encoder->setVertexBytes(&uniforms, sizeof uniforms, 1);
  for (const auto& p : asset_->primitives) {
    encoder->setVertexBuffer(p.vertices.get(), 0, 0);
    encoder->setFragmentTexture(p.texture.get(), 0);
    encoder->setFragmentBytes(&p.material, sizeof p.material, 0);
    encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, p.index_count, MTL::IndexTypeUInt32, p.indices.get(), 0);
  }
}
