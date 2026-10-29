#include "core/metal_ptr.hpp"
#include "draw/tornado_draw.hpp"

#include <SDL3/SDL.h>
#include <cmath>
#include <numbers>

namespace {
struct Uniforms {
  simd_float4 center_radius;
  simd_float4 aspect_phase;
};
static_assert(sizeof(Uniforms) == 32);
}

bool TornadoDraw::Initialize(MTL::Device* device, MTL::Library* library, MTL::PixelFormat format) {
  if (!device || !library || !tornado_ || pipeline_.get()) return false;
  auto vertex = quacknado::metal::adopt(library->newFunction(NS::String::string("vertex_tornado", NS::UTF8StringEncoding)));
  auto fragment = quacknado::metal::adopt(library->newFunction(NS::String::string("fragment_tornado", NS::UTF8StringEncoding)));
  if (!vertex.get() || !fragment.get()) return false;
  auto descriptor = quacknado::metal::adopt(MTL::RenderPipelineDescriptor::alloc()->init());
  descriptor->setVertexFunction(vertex.get());
  descriptor->setFragmentFunction(fragment.get());
  descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
  auto* color = descriptor->colorAttachments()->object(0);
  color->setPixelFormat(format);
  color->setBlendingEnabled(true);
  color->setSourceRGBBlendFactor(MTL::BlendFactorOne);
  color->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
  color->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
  color->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
  NS::Error* error = nullptr;
  pipeline_ = quacknado::metal::adopt(device->newRenderPipelineState(descriptor.get(), &error));
  if (!pipeline_.get()) {
    SDL_Log("Tornado marker pipeline failed: %s", error ? error->localizedDescription()->utf8String() : "unknown error");
    return false;
  }
  auto depth = quacknado::metal::adopt(MTL::DepthStencilDescriptor::alloc()->init());
  depth->setDepthCompareFunction(MTL::CompareFunctionLessEqual);
  depth->setDepthWriteEnabled(false);
  depth_state_ = quacknado::metal::adopt(device->newDepthStencilState(depth.get()));
  return depth_state_.get() != nullptr;
}

void TornadoDraw::Record(MTL::RenderCommandEncoder* encoder, int width, int height) {
  if (!encoder || !pipeline_.get() || width <= 0 || height <= 0) return;
  const auto center = tornado_->center();
  constexpr double speed = 2.0 * std::numbers::pi / 40.0;
  const float phase = static_cast<float>(std::fmod(double(SDL_GetTicksNS()) * 1e-9 * speed,
                                                 2.0 * std::numbers::pi));
  Uniforms uniforms {{center.x, center.y + 0.04f, center.z, tornado_->bottom_radius()},
                     {float(width) / float(height), phase, 0, 0}};
  encoder->setRenderPipelineState(pipeline_.get());
  encoder->setDepthStencilState(depth_state_.get());
  encoder->setCullMode(MTL::CullModeNone);
  encoder->setVertexBytes(&uniforms, sizeof uniforms, 0);
  encoder->setFragmentBytes(&uniforms, sizeof uniforms, 0);
  encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6));
}
