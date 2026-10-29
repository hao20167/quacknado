#include "core/metal_ptr.hpp"
#include "draw/model_draw.hpp"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstdio>
#include <stdexcept>
void glb_benchmark(size_t count, int repetitions, const char *model_name) {
  auto pool = quacknado::metal::adopt(NS::AutoreleasePool::alloc()->init());
  auto device = quacknado::metal::adopt(MTL::CreateSystemDefaultDevice());
  if (!device)
    throw std::runtime_error("Metal unavailable");
  auto queue = quacknado::metal::adopt(device->newCommandQueue());
  NS::Error *error = nullptr;
  const char *base = SDL_GetBasePath();
  if (!base)
    throw std::runtime_error("Cannot locate executable shader directory");
  const auto shader_path = std::string(base) + "object.metallib";
  auto lib = quacknado::metal::adopt(
      device->newLibrary(NS::String::string(shader_path.c_str(), NS::UTF8StringEncoding), &error));
  if (!lib)
    throw std::runtime_error("Build shaders before GLB benchmark");
  std::vector<Chicken> objects;
  objects.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    objects.emplace_back(simd_float3{float(i % 25) * 1.4f - 17, 1, float(i / 25) * 1.4f - 8});
    objects.back().body_id_ = JPH::BodyID(uint32_t(i));
    objects.back().orientation_ = simd_quaternion(float(i % 7) * .3f, simd_float3{0, 1, 0});
  }
  ModelDraw model(&objects[0], std::string("assets/models/") + model_name + ".glb");
  for (size_t i = 1; i < count; ++i)
    model.AddObject(&objects[i]);
  if (!model.Initialize(device.get(), lib.get(), MTL::PixelFormatRGBA8Unorm))
    throw std::runtime_error("GLB initialize failed");
  auto dd =
      MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatDepth32Float, 256, 256, false);
  dd->setUsage(MTL::TextureUsageRenderTarget);
  dd->setStorageMode(MTL::StorageModePrivate);
  auto depth = quacknado::metal::adopt(device->newTexture(dd));
  auto ds = quacknado::metal::adopt(MTL::DepthStencilDescriptor::alloc()->init());
  ds->setDepthCompareFunction(MTL::CompareFunctionLess);
  ds->setDepthWriteEnabled(true);
  auto depthState = quacknado::metal::adopt(device->newDepthStencilState(ds.get()));
  auto td =
      MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRGBA8Unorm, 256, 256, false);
  td->setUsage(MTL::TextureUsageRenderTarget);
  td->setStorageMode(MTL::StorageModeShared);
  auto texture = quacknado::metal::adopt(device->newTexture(td));
  std::vector<unsigned char> expected(256 * 256 * 4), pixels(expected.size());
  for (int mode = 0; mode < 2; ++mode)
    for (int r = -3; r < repetitions; ++r) {
      auto framePool = quacknado::metal::adopt(NS::AutoreleasePool::alloc()->init());
      auto begin = std::chrono::steady_clock::now();
      auto pass = MTL::RenderPassDescriptor::renderPassDescriptor();
      auto attachment = pass->colorAttachments()->object(0);
      attachment->setTexture(texture.get());
      attachment->setLoadAction(MTL::LoadActionClear);
      attachment->setStoreAction(MTL::StoreActionStore);
      attachment->setClearColor(MTL::ClearColor(0, 0, 0, 1));
      pass->depthAttachment()->setTexture(depth.get());
      pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
      pass->depthAttachment()->setStoreAction(MTL::StoreActionDontCare);
      pass->depthAttachment()->setClearDepth(1);
      auto command = queue->commandBuffer();
      auto encoder = command->renderCommandEncoder(pass);
      encoder->setDepthStencilState(depthState.get());
      model.instancing = mode == 1;
      model.Record(encoder, 256, 256);
      encoder->endEncoding();
      command->commit();
      auto submitted = std::chrono::steady_clock::now();
      command->waitUntilCompleted();
      auto end = std::chrono::steady_clock::now();
      if (command->status() == MTL::CommandBufferStatusError)
        throw std::runtime_error("Render command failed");
      texture->getBytes(pixels.data(), 256 * 4, MTL::Region(0, 0, 256, 256), 0);
      bool nonempty = false;
      for (size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] || pixels[i + 1] || pixels[i + 2]) {
          nonempty = true;
          break;
        }
      if (!nonempty)
        throw std::runtime_error("GLB render unexpectedly empty");
      if (mode == 0)
        expected = pixels;
      else if (pixels != expected)
        throw std::runtime_error("Instancing image mismatch");
      if (r >= 0) {
        auto ns = [](auto d) { return std::chrono::duration<double, std::nano>(d).count(); };
        std::printf("glb_render_%s,%s,%zu,%d,%.0f,%.0f,0\n", mode ? "instanced" : "individual",
                    model_name, count, r, ns(end - begin),
                    (command->GPUEndTime() - command->GPUStartTime()) * 1e9);
        std::printf("glb_draws_%s,%s,%zu,%d,%zu,0,0\n", mode ? "instanced" : "individual",
                    model_name, count, r, model.last_draw_calls);
        std::printf("glb_submit_%s,%s,%zu,%d,%.0f,0,0\n", mode ? "instanced" : "individual",
                    model_name, count, r, ns(submitted - begin));
      }
    }
}
