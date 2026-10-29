#include "assets/glb_asset.hpp"
#include "core/metal_ptr.hpp"
#include "draw/model_draw.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
constexpr int width = 384, height = 256;
using Pixels = std::vector<unsigned char>;
struct Frame {
  quacknado::metal::Ptr<MTL::Texture> color, depth;
  quacknado::metal::Ptr<MTL::CommandBuffer> command;
  Pixels pixels() const {
    Pixels out(width * height * 4);
    color->getBytes(out.data(), width * 4, MTL::Region(0, 0, width, height), 0);
    return out;
  }
};
Frame target(MTL::Device *device) {
  Frame f;
  auto c =
      MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRGBA8Unorm, width, height, false);
  c->setStorageMode(MTL::StorageModeShared);
  c->setUsage(MTL::TextureUsageRenderTarget);
  f.color = quacknado::metal::adopt(device->newTexture(c));
  auto d = MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatDepth32Float, width, height,
                                                       false);
  d->setStorageMode(MTL::StorageModePrivate);
  d->setUsage(MTL::TextureUsageRenderTarget);
  f.depth = quacknado::metal::adopt(device->newTexture(d));
  if (!f.color || !f.depth)
    throw std::runtime_error("Texture allocation failed");
  return f;
}
void wait(Frame &f) {
  f.command->waitUntilCompleted();
  if (f.command->status() == MTL::CommandBufferStatusError)
    throw std::runtime_error("GPU command failed");
}
} // namespace
int main(int argc, char **argv) {
  try {
    auto pool = quacknado::metal::adopt(NS::AutoreleasePool::alloc()->init());
    auto device = quacknado::metal::adopt(MTL::CreateSystemDefaultDevice());
    if (!device)
      throw std::runtime_error("Metal unavailable");
    auto queue = quacknado::metal::adopt(device->newCommandQueue());
    NS::Error *error = nullptr;
    auto lib = quacknado::metal::adopt(device->newLibrary(
        NS::String::string(argc > 1 ? argv[1] : "build/perf/Release/object.metallib",
                           NS::UTF8StringEncoding),
        &error));
    if (!lib)
      throw std::runtime_error("Shader library unavailable");
    auto ds = quacknado::metal::adopt(MTL::DepthStencilDescriptor::alloc()->init());
    ds->setDepthWriteEnabled(true);
    ds->setDepthCompareFunction(MTL::CompareFunctionLess);
    auto depthState = quacknado::metal::adopt(device->newDepthStencilState(ds.get()));
    const char *fixtureNames[] = {"chicken", "creeper", "chest"};
    std::array<size_t, 3> primitiveCounts;
    for (size_t i = 0; i < 3; ++i)
      primitiveCounts[i] =
          quacknado::LoadGlb(std::string("assets/models/") + fixtureNames[i] + ".glb", device.get())
              .primitives.size();
    std::puts(
        "objects,filtering,submitted_before_wait,pixel_match,individual_draws,instanced_draws");
    for (size_t n : {size_t(0), size_t(1), size_t(237), size_t(240), size_t(243), size_t(600)})
      for (bool filtering : {false, true}) {
        auto scenarioPool = quacknado::metal::adopt(NS::AutoreleasePool::alloc()->init());
        std::vector<IObjModule> objects(n);
        const char *names[] = {"chicken", "creeper", "chest"};
        std::vector<std::unique_ptr<ModelDraw>> original;
        std::array<std::unique_ptr<ModelDraw>, 3> grouped;
        for (size_t i = 0; i < n; ++i) {
          auto &o = objects[i];
          o.visual_height_ = 1.5f;
          auto path = std::string("assets/models/") + names[i % 3] + ".glb";
          original.push_back(std::make_unique<ModelDraw>(&o, path));
          original.back()->instancing = false;
          if (!original.back()->Initialize(device.get(), lib.get(), MTL::PixelFormatRGBA8Unorm))
            throw std::runtime_error("Original initialization failed");
          if (!grouped[i % 3])
            grouped[i % 3] = std::make_unique<ModelDraw>(&o, path);
          else
            grouped[i % 3]->AddObject(&o);
        }
        for (auto &m : grouped)
          if (m && !m->Initialize(device.get(), lib.get(), MTL::PixelFormatRGBA8Unorm))
            throw std::runtime_error("Group initialization failed");
        auto pose = [&](int frame) {
          for (size_t i = 0; i < n; ++i) {
            auto &o = objects[i];
            o.position_ = {float(i % 25) * 1.4f - 17 + .07f * frame, 1 + .1f * float(i % 3),
                           float(i / 25) * 1.4f - 8 + .031f * float(i % 7)};
            o.orientation_ =
                simd_quaternion(.13f * float(i % 7) + .17f * frame, simd_float3{0, 1, 0});
            o.awaiting_spawn_ = filtering && (i % 11 == 0);
            o.body_id_ = filtering && (i % 13 == 0) ? JPH::BodyID() : JPH::BodyID(uint32_t(i));
          }
        };
        size_t individualCalls = 0, instancedCalls = 0;
        auto encode = [&](Frame &f, bool batched) {
          auto pass = MTL::RenderPassDescriptor::renderPassDescriptor();
          auto c = pass->colorAttachments()->object(0);
          c->setTexture(f.color.get());
          c->setLoadAction(MTL::LoadActionClear);
          c->setStoreAction(MTL::StoreActionStore);
          c->setClearColor(MTL::ClearColor(0, 0, 0, 1));
          pass->depthAttachment()->setTexture(f.depth.get());
          pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
          pass->depthAttachment()->setClearDepth(1);
          pass->depthAttachment()->setStoreAction(MTL::StoreActionDontCare);
          f.command = quacknado::metal::retain(queue->commandBuffer());
          auto encoder = f.command->renderCommandEncoder(pass);
          encoder->setDepthStencilState(depthState.get());
          if (batched) {
            for (auto &m : grouped)
              if (m) {
                m->Record(encoder, width, height);
                instancedCalls += m->last_draw_calls;
              }
          } else {
            for (auto &m : original) {
              m->Record(encoder, width, height);
              individualCalls += m->last_draw_calls;
            }
          }
          size_t submittedObjects = 0, submittedInstances = 0, expected = 0, expectedInstances = 0;
          for (size_t i = 0; i < objects.size(); ++i) {
            const auto &object = objects[i];
            if (!object.awaiting_spawn_ && !object.body_id_.IsInvalid()) {
              ++expected;
              expectedInstances += primitiveCounts[i % 3];
            }
          }
          auto count = [&](const auto &module) {
            if (module) {
              const auto stats = module->SubmittedModels();
              submittedObjects += stats.objects;
              submittedInstances += stats.instances;
            }
          };
          if (batched) {
            for (const auto &m : grouped)
              count(m);
          } else {
            for (const auto &m : original)
              count(m);
          }
          // Use loaded asset metadata: models can contain multiple primitives.
          // Check counters independently of batching, skipped objects and pixels.
          if (submittedObjects != expected || submittedInstances != expectedInstances)
            throw std::runtime_error("Submitted model/instance counter mismatch");
          encoder->endEncoding();
          f.command->commit();
        };
        std::array<Pixels, 3> reference;
        for (int frame = 0; frame < 3; ++frame) {
          pose(frame);
          auto f = target(device.get());
          encode(f, false);
          wait(f);
          reference[frame] = f.pixels();
        }
        if (n > 1) {
          bool nonempty = false;
          for (size_t i = 0; i < reference[0].size(); i += 4)
            if (reference[0][i] || reference[0][i + 1] || reference[0][i + 2]) {
              nonempty = true;
              break;
            }
          if (!nonempty)
            throw std::runtime_error("Reference mixed scene is empty");
          if (reference[0] == reference[2])
            throw std::runtime_error("Animated reference did not change pixels");
        }
        std::array<Frame, 3> flight;
        for (int frame = 0; frame < 3; ++frame) {
          pose(frame);
          flight[frame] = target(device.get());
          encode(flight[frame], true);
        }
        // Mutate CPU data before waiting: command-owned transform copies must survive.
        for (auto &o : objects)
          o.position_ = {999, 999, 999};
        for (int frame = 0; frame < 3; ++frame) {
          wait(flight[frame]);
          if (flight[frame].pixels() != reference[frame])
            throw std::runtime_error("Mixed GLB pixel mismatch N=" + std::to_string(n) +
                                     " frame=" + std::to_string(frame));
        }
        std::printf("%zu,%d,3,true,%zu,%zu\n", n, int(filtering), individualCalls, instancedCalls);
      }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
