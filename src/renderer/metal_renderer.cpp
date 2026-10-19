#include "metal_renderer.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPass.hpp"
#include "QuartzCore/CAMetalDrawable.hpp"

#include <SDL3/SDL.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.hpp>

namespace quacknado {

MetalRenderer::~MetalRenderer() {
  if (metal_view_) {
    SDL_Metal_DestroyView(metal_view_);
    metal_view_ = nullptr;
    metal_layer_ = nullptr;
  }
}

bool MetalRenderer::Initialize(SDL_Window* window) {
  window_ = window;
  
  metal_view_ = SDL_Metal_CreateView(window_);
  if (metal_view_ == nullptr) {
    SDL_Log("SDL_Metal_CreateView failed: %s\n", SDL_GetError());
    return false;
  }

  void* layer_pointer = SDL_Metal_GetLayer(metal_view_);
  if (layer_pointer == nullptr) {
    SDL_Log("SDL_Metal_GetLayer failed\n");
    return false;
  }
  metal_layer_ = static_cast<CA::MetalLayer*>(layer_pointer);

  device_ = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
  if (!device_) {
    SDL_Log("CreateSystemDefaultDevice failed\n");
    return false;
  }

  metal_layer_->setDevice(device_.get());
  metal_layer_->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  metal_layer_->setFramebufferOnly(true);

  command_queue_ = NS::TransferPtr(device_->newCommandQueue());
  if (!command_queue_) {
    SDL_Log("Failed to create Metal command queue\n");
    return false;
  }

  Resize();
  return true;
}

void MetalRenderer::Resize() {
  if (window_ == nullptr || metal_layer_ == nullptr) return;

  int width = 0, height = 0;
  if (!SDL_GetWindowSize(window_, &width, &height)) {
    SDL_Log("SDL_GetWindowSize failed: %s\n", SDL_GetError());
    return;
  }

  drawable_extent_ = {
    .width = width,
    .height = height
  };
  if (!drawable_extent_.IsValid()) {
    return;
  }

  const CGSize drawable_size {
    static_cast<CGFloat>(width),
    static_cast<CGFloat>(height)
  };
  metal_layer_->setDrawableSize(drawable_size);
}

void MetalRenderer::Render() {
  if (metal_layer_ == nullptr || command_queue_.get() == nullptr || !drawable_extent_.IsValid()) {
    return;
  }
  
  auto autorelease_pool = TransferPtr(NS::AutoreleasePool::alloc()->init());

  CA::MetalDrawable* drawable = metal_layer_->nextDrawable();
  if (drawable == nullptr) {
    return;
  }
  
  MTL::CommandBuffer* command_buffer = command_queue_->commandBuffer();
  if (command_buffer == nullptr) {
    return;
  }

  MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor* color_attachment = pass->colorAttachments()->object(0);
  color_attachment->setTexture(drawable->texture());
  color_attachment->setLoadAction(MTL::LoadActionClear);
  color_attachment->setStoreAction(MTL::StoreActionStore);
  color_attachment->setClearColor(MTL::ClearColor(0.08, 0.12, 0.18, 1.0));

  MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass);
  if (encoder == nullptr) {
    return;
  }
  encoder->endEncoding();

  command_buffer->presentDrawable(drawable);
  command_buffer->commit();
}

} // namespace quacknado
