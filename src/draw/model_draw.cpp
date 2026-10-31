#include "draw/model_draw.hpp"
#include "assets/glb_asset.hpp"
#include "core/metal_ptr.hpp"

#include <SDL3/SDL.h>
#include <array>
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
} // namespace
struct ModelDraw::Asset {
  struct Primitive {
    quacknado::metal::Ptr<MTL::Buffer> vertices, indices;
    quacknado::metal::Ptr<MTL::Texture> texture;
    size_t index_count;
    MaterialUniforms material;
  };
  std::vector<Primitive> primitives;
};

ModelDraw::ModelDraw(IObjModule *object, std::string path)
    : objects_{object}, path_(std::move(path)) {}
ModelDraw::~ModelDraw() = default;

bool ModelDraw::Initialize(MTL::Device *device, MTL::Library *library, MTL::PixelFormat format) {
  if (!device || !library || pipeline_.get())
    return false;
  try {
    using Key = std::pair<MTL::Device *, std::string>;
    static std::map<Key, std::weak_ptr<Asset>> cache;
    static std::map<std::tuple<MTL::Device *, MTL::Library *, MTL::PixelFormat>,
                    quacknado::metal::Ptr<MTL::RenderPipelineState>>
        pipelines;
    const auto pipeline_key = std::make_tuple(device, library, format);
    auto &pipeline = pipelines[pipeline_key];
    if (!pipeline.get()) {
      auto vertex = quacknado::metal::adopt(
          library->newFunction(NS::String::string("vertex_model", NS::UTF8StringEncoding)));
      auto fragment = quacknado::metal::adopt(
          library->newFunction(NS::String::string("fragment_model", NS::UTF8StringEncoding)));
      if (!vertex.get() || !fragment.get())
        throw std::runtime_error("Model shader functions missing");
      auto descriptor = quacknado::metal::adopt(MTL::RenderPipelineDescriptor::alloc()->init());
      descriptor->setVertexFunction(vertex.get());
      descriptor->setFragmentFunction(fragment.get());
      descriptor->colorAttachments()->object(0)->setPixelFormat(format);
      descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
      NS::Error *error = nullptr;
      pipeline = quacknado::metal::adopt(device->newRenderPipelineState(descriptor.get(), &error));
      if (!pipeline.get())
        throw std::runtime_error(error ? error->localizedDescription()->utf8String()
                                       : "Model pipeline failed");
    }
    pipeline_ = pipeline;
    auto &cached = cache[{device, path_}];
    asset_ = cached.lock();
    if (!asset_) {
      auto loaded = quacknado::LoadGlb(path_, device);
      asset_ = std::make_shared<Asset>();
      for (auto &p : loaded.primitives) {
        Asset::Primitive primitive;
        primitive.vertices = quacknado::metal::adopt(
            device->newBuffer(p.vertices.data(), p.vertices.size() * sizeof(quacknado::ModelVertex),
                              MTL::ResourceStorageModeShared));
        primitive.indices = quacknado::metal::adopt(device->newBuffer(
            p.indices.data(), p.indices.size() * sizeof(uint32_t), MTL::ResourceStorageModeShared));
        if (!primitive.vertices.get() || !primitive.indices.get())
          throw std::runtime_error("Model buffer allocation failed");
        primitive.texture = std::move(p.texture);
        primitive.index_count = p.indices.size();
        primitive.material = {p.base_color, {p.alpha_cutoff, 0, 0, 0}};
        asset_->primitives.push_back(std::move(primitive));
      }
      cached = asset_;
      SDL_Log("Loaded %s (%zu primitives)", path_.c_str(), asset_->primitives.size());
    }
    return true;
  } catch (const std::exception &error) {
    SDL_Log("Model load failed [%s]: %s", path_.c_str(), error.what());
    return false;
  }
}

void ModelDraw::Record(MTL::RenderCommandEncoder *encoder, int width, int height) {
  last_draw_calls = 0;
  last_models = {};
  if (!encoder || !asset_ || height <= 0)
    return;
  // setVertexBytes copies each small batch into command-owned storage. No shared
  // mutable instance buffer can be overwritten while prior frames are in flight.
  constexpr size_t capacity = 80;
  static_assert(sizeof(Uniforms) * capacity <= 4096);
  std::array<Uniforms, capacity> batch;
  size_t count = 0;
  encoder->setRenderPipelineState(pipeline_.get());
  encoder->setCullMode(MTL::CullModeNone);
  auto flush = [&] {
    if (!count)
      return;
    encoder->setVertexBytes(batch.data(), count * sizeof(Uniforms), 1);
    for (const auto &p : asset_->primitives) {
      encoder->setVertexBuffer(p.vertices.get(), 0, 0);
      encoder->setFragmentTexture(p.texture.get(), 0);
      encoder->setFragmentBytes(&p.material, sizeof p.material, 0);
      encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, p.index_count,
                                     MTL::IndexTypeUInt32, p.indices.get(), 0, count);
      ++last_draw_calls;
      last_models.instances += count;
    }
    count = 0;
  };
  for (const auto *object : objects_) {
    if (!object || object->awaiting_spawn_ || object->body_id_.IsInvalid())
      continue;
    ++last_models.objects;
    const float scale = object->visual_height_ * 0.5f;
    batch[count++] = {
        object->orientation_.vector,
        {object->position_.x, object->position_.y, object->position_.z, float(width) / height},
        {scale, scale, scale, 0}};
    if (count == (instancing ? capacity : 1))
      flush();
  }
  flush();
}
