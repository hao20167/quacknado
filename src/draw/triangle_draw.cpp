#include <Metal/Metal.hpp>
#include <Foundation/Foundation.hpp>
#include <SDL3/SDL.h>

#include "draw/triangle_draw.hpp"

TriangleDraw::~TriangleDraw() {

}

bool TriangleDraw::Initialize(MTL::Device* device, MTL::Library* library, MTL::PixelFormat color_format) {
  if (device == nullptr || library == nullptr || pipeline_.get()) {
    return false;
  }
  auto vertex_function = NS::TransferPtr(library->newFunction(
    NS::String::string("vertexMain", NS::UTF8StringEncoding)
  ));
  auto fragment_function = NS::TransferPtr(library->newFunction(
    NS::String::string("fragmentMain", NS::UTF8StringEncoding)
  ));
  if (!vertex_function.get() || !fragment_function.get()) {
    SDL_Log("Triangle shader functions cannot be initialized.\n");
    return false;
  }

  auto descriptor = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
  descriptor->setVertexFunction(vertex_function.get());
  descriptor->setFragmentFunction(fragment_function.get());
  descriptor->colorAttachments()->object(0)->setPixelFormat(color_format);
  NS::Error* error = nullptr;
  pipeline_ = NS::TransferPtr(device->newRenderPipelineState(descriptor.get(), &error));
  if (!pipeline_.get()) {
    SDL_Log("Triangle Pipeline failed.\n");
    if (error) {
      SDL_Log("%s\n", error->localizedDescription()->utf8String());
    }
    return false;
  }

  return true;
}

void TriangleDraw::Record(MTL::RenderCommandEncoder* encoder) {
  if (!encoder || !pipeline_.get()) {
    return;
  }
  encoder->setRenderPipelineState(pipeline_.get());
  encoder->setVertexBytes(&offset_, sizeof offset_, 0);
  encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), 3);
}
