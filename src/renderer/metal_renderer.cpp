#include <SDL3/SDL.h>
#include <CoreGraphics/CoreGraphics.h>
#include <QuartzCore/QuartzCore.hpp>
#include <Metal/Metal.hpp>

#include "metal_renderer.hpp"
#include "Foundation/NSSharedPtr.hpp"
#include "Metal/MTLDepthStencil.hpp"
#include "renderer/render_module.hpp"

namespace quacknado {

MetalRenderer::~MetalRenderer() {
  if (command_queue_.get()) {
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    auto buffer = command_queue_->commandBuffer();
    if (buffer) {
      buffer->commit();
      buffer->waitUntilCompleted();
    }
  }
  if (metal_view_) {
    SDL_Metal_DestroyView(metal_view_);
    metal_view_ = nullptr;
    metal_layer_ = nullptr;
  }
}

void MetalRenderer::AddModule(std::unique_ptr<IRenderModule> module) {
  modules_.push_back(std::move(module));
}

bool MetalRenderer::Initialize(SDL_Window* window, const std::string& metallib_path) {
  window_ = window;
  
  metal_view_ = SDL_Metal_CreateView(window_);
  if (!metal_view_) {
    SDL_Log("SDL_Metal_CreateView failed: %s\n", SDL_GetError());
    return false;
  }

  metal_layer_ = static_cast<CA::MetalLayer*>(SDL_Metal_GetLayer(metal_view_));
  if (!metal_layer_) {
    SDL_Log("SDL_Metal_GetLayer failed\n");
    return false;
  }

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

  auto autorelease_pool = TransferPtr(NS::AutoreleasePool::alloc()->init());

  NS::Error* error = nullptr;
  NS::String* path = NS::String::string(metallib_path.c_str(), NS::UTF8StringEncoding);
  auto library = NS::TransferPtr(device_->newLibrary(path, &error));
  if (!library.get()) {
    SDL_Log("Could not load metallib: %s", metallib_path.c_str());
    if (error) {
      SDL_Log("%s", error->localizedDescription()->utf8String());
    }

    return false;
  }

  auto depth_desc = NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());
  depth_desc->setDepthCompareFunction(MTL::CompareFunctionLess);
  depth_desc->setDepthWriteEnabled(true);
  depth_state_ = NS::TransferPtr(device_->newDepthStencilState(depth_desc.get()));

  for (auto& module: modules_) {
    if (!module->Initialize(device_.get(), library.get(), metal_layer_->pixelFormat())) {
      return false;
    }
  }

  Resize();
  return true;
}

void MetalRenderer::Resize() {
  if (window_ == nullptr || metal_layer_ == nullptr) return;

  int width = 0, height = 0;
  if (!SDL_GetWindowSizeInPixels(window_, &width, &height)) {
    SDL_Log("SDL_GetWindowSizeInPixels failed: %s\n", SDL_GetError());
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
  const CGSize old_size = metal_layer_->drawableSize();
  if (old_size.width == drawable_size.width && old_size.height == drawable_size.height) {
    return;
  }

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
  MTL::RenderPassColorAttachmentDescriptor* color = pass->colorAttachments()->object(0);
  color->setTexture(drawable->texture());
  color->setLoadAction(MTL::LoadActionClear);
  color->setStoreAction(MTL::StoreActionStore);
  color->setClearColor(MTL::ClearColor(0.05, 0.06, 0.10, 1.0));

  const auto width = drawable->texture()->width();
  const auto height = drawable->texture()->height();

  if (!depth_texture_.get() || depth_texture_->width() != width || depth_texture_->height() != height) {
    auto* td = MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatDepth32Float, width, height, false);
    td->setUsage(MTL::TextureUsageRenderTarget);
    td->setStorageMode(MTL::StorageModePrivate);
    depth_texture_ = NS::TransferPtr(device_->newTexture(td));
  }

  auto* depth = pass->depthAttachment();
  depth->setTexture(depth_texture_.get());
  depth->setLoadAction(MTL::LoadActionClear);
  depth->setStoreAction(MTL::StoreActionDontCare);
  depth->setClearDepth(1.0);

  MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass);
  if (encoder == nullptr) {
    return;
  }

  encoder->setDepthStencilState(depth_state_.get());
  for (auto& module: modules_) {
    module->Record(encoder, width, height);
  }

  encoder->endEncoding();

  command_buffer->presentDrawable(drawable);
  command_buffer->commit();
}

} // namespace quacknado
