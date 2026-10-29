#include "metal_renderer.hpp"
#include "core/metal_ptr.hpp"

#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <SDL3/SDL.h>
#include <chrono>
#include <stdexcept>

#include "Metal/MTLPixelFormat.hpp"
#include "render_module.hpp"

namespace quacknado {

MetalRenderer::~MetalRenderer() {
  if (command_queue_.get()) {
    auto pool = quacknado::metal::adopt(NS::AutoreleasePool::alloc()->init());
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

bool MetalRenderer::Initialize(SDL_Window *window, const std::string &metallib_path) {
  window_ = window;

  metal_view_ = SDL_Metal_CreateView(window_);
  if (!metal_view_) {
    SDL_Log("SDL_Metal_CreateView failed: %s\n", SDL_GetError());
    return false;
  }

  metal_layer_ = static_cast<CA::MetalLayer *>(SDL_Metal_GetLayer(metal_view_));
  if (!metal_layer_) {
    SDL_Log("SDL_Metal_GetLayer failed\n");
    return false;
  }

  device_ = quacknado::metal::adopt(MTL::CreateSystemDefaultDevice());
  if (!device_) {
    SDL_Log("CreateSystemDefaultDevice failed\n");
    return false;
  }

  metal_layer_->setDevice(device_.get());
  metal_layer_->setPixelFormat(MTL::PixelFormatBGRA8Unorm_sRGB);
  metal_layer_->setFramebufferOnly(true);

  command_queue_ = quacknado::metal::adopt(device_->newCommandQueue());
  if (!command_queue_) {
    SDL_Log("Failed to create Metal command queue\n");
    return false;
  }

  auto autorelease_pool = quacknado::metal::adopt(NS::AutoreleasePool::alloc()->init());

  NS::Error *error = nullptr;
  NS::String *path = NS::String::string(metallib_path.c_str(), NS::UTF8StringEncoding);
  auto library = quacknado::metal::adopt(device_->newLibrary(path, &error));
  if (!library.get()) {
    SDL_Log("Could not load metallib: %s", metallib_path.c_str());
    if (error) {
      SDL_Log("%s", error->localizedDescription()->utf8String());
    }

    return false;
  }

  auto depth_desc = quacknado::metal::adopt(MTL::DepthStencilDescriptor::alloc()->init());
  depth_desc->setDepthCompareFunction(MTL::CompareFunctionLess);
  depth_desc->setDepthWriteEnabled(true);
  depth_state_ = quacknado::metal::adopt(device_->newDepthStencilState(depth_desc.get()));

  for (auto &module : modules_) {
    if (!module->Initialize(device_.get(), library.get(), metal_layer_->pixelFormat())) {
      return false;
    }
  }

  Resize();
  return true;
}

void MetalRenderer::Resize() {
  if (window_ == nullptr || metal_layer_ == nullptr)
    return;

  int width = 0, height = 0;
  if (!SDL_GetWindowSizeInPixels(window_, &width, &height)) {
    SDL_Log("SDL_GetWindowSizeInPixels failed: %s\n", SDL_GetError());
    return;
  }

  drawable_extent_ = {.width = width, .height = height};
  if (!drawable_extent_.IsValid()) {
    return;
  }

  const CGSize drawable_size{static_cast<CGFloat>(width), static_cast<CGFloat>(height)};
  const CGSize old_size = metal_layer_->drawableSize();
  if (old_size.width == drawable_size.width && old_size.height == drawable_size.height) {
    return;
  }

  metal_layer_->setDrawableSize(drawable_size);
}

void MetalRenderer::Render() {
  last_submitted = false;
  last_models = {};
  if (metal_layer_ == nullptr || command_queue_.get() == nullptr || !drawable_extent_.IsValid()) {
    return;
  }

  auto autorelease_pool = quacknado::metal::adopt(NS::AutoreleasePool::alloc()->init());

  CA::MetalDrawable *drawable = metal_layer_->nextDrawable();
  if (drawable == nullptr) {
    return;
  }

  auto encode_begin =
      profiling ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
  MTL::CommandBuffer *command_buffer = command_queue_->commandBuffer();
  if (command_buffer == nullptr) {
    return;
  }

  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor *color = pass->colorAttachments()->object(0);
  color->setTexture(drawable->texture());
  color->setLoadAction(MTL::LoadActionClear);
  color->setStoreAction(MTL::StoreActionStore);
  color->setClearColor(MTL::ClearColor(0.05, 0.06, 0.10, 1.0));

  const auto width = drawable->texture()->width();
  const auto height = drawable->texture()->height();

  if (!depth_texture_.get() || depth_texture_->width() != width ||
      depth_texture_->height() != height) {
    auto *td = MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatDepth32Float, width,
                                                           height, false);
    td->setUsage(MTL::TextureUsageRenderTarget);
    td->setStorageMode(MTL::StorageModePrivate);
    depth_texture_ = quacknado::metal::adopt(device_->newTexture(td));
  }

  auto *depth = pass->depthAttachment();
  depth->setTexture(depth_texture_.get());
  depth->setLoadAction(MTL::LoadActionClear);
  depth->setStoreAction(MTL::StoreActionDontCare);
  depth->setClearDepth(1.0);

  MTL::RenderCommandEncoder *encoder = command_buffer->renderCommandEncoder(pass);
  if (encoder == nullptr) {
    return;
  }

  encoder->setDepthStencilState(depth_state_.get());
  for (auto &module : modules_) {
    module->Record(encoder, width, height);
    const auto counts = module->SubmittedModels();
    last_models.objects += counts.objects;
    last_models.instances += counts.instances;
  }

  encoder->endEncoding();

  command_buffer->presentDrawable(drawable);
  command_buffer->commit();
  last_submitted = true;
  if (profiling) {
    last_submission_ns =
        std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - encode_begin)
            .count();
    pending_profile_.push_back(metal::retain(command_buffer));
    if (pending_profile_.size() >= 3)
      RetireProfileFrame();
  }
}

void MetalRenderer::RetireProfileFrame() {
  auto &command = pending_profile_.front();
  command->waitUntilCompleted();
  if (command->status() == MTL::CommandBufferStatusError)
    throw std::runtime_error("Replay GPU command failed");
  gpu_ns_.push_back((command->GPUEndTime() - command->GPUStartTime()) * 1e9);
  pending_profile_.pop_front();
}
const std::vector<double> &MetalRenderer::FinishProfile() {
  while (!pending_profile_.empty())
    RetireProfileFrame();
  return gpu_ns_;
}
} // namespace quacknado
