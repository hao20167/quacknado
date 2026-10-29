#include "gpu.hpp"
#include "core/metal_ptr.hpp"
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <cstring>
#include <stdexcept>
struct GPU::Impl {
  quacknado::metal::Ptr<MTL::Device> device;
  quacknado::metal::Ptr<MTL::CommandQueue> queue;
  quacknado::metal::Ptr<MTL::ComputePipelineState> pipeline;
  quacknado::metal::Ptr<MTL::Buffer> input, output;
};
GPU::GPU(size_t n) : impl_(std::make_unique<Impl>()) {
  auto &g = *impl_;
  g.device = quacknado::metal::adopt(MTL::CreateSystemDefaultDevice());
  if (!g.device)
    throw std::runtime_error("Metal device unavailable");
  g.queue = quacknado::metal::adopt(g.device->newCommandQueue());
  const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct Sample {float3 p,v;};
float sm(float a,float b,float x){float t=clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
kernel void wind(device const Sample* a [[buffer(0)]],device float3* out [[buffer(1)]],constant uint& n [[buffer(2)]],uint i [[thread_position_in_grid]]) {
 if(i>=n)return;
 float3 p=a[i].p-float3(0,0,-8),v=a[i].v;
 if(p.y<0 || p.y>=30){out[i]=0;return;}
 float u=p.y/30,r=length(p.xz),t=r/(10+20*u);
 if(t>=1.5f){out[i]=0;return;}
 float inv=r>1e-5f?1/r:0;
 float3 radial=float3(p.x*inv,0,p.z*inv),tangent=float3(-radial.z,0,radial.x);
 float top=sm(.7f,.95f,u),side=1-sm(1,1.5f,t),ti=1-sm(.95f,1,u);
 float3 wind=tangent*50*(t<=1?t:1/t)*(1-.5f*top)+radial*(-100*(1-top)+25*top)*min(t,1.f);
 float3 f=(wind-v)*3*side*ti;
 const float heights[]={0,.2f,.5f,.9f,1},strengths[]={.12f,.45f,1,.25f,0};
 float lift=0;
 for(int j=1;j<5;++j)if(u<=heights[j]){lift=mix(strengths[j-1],strengths[j],sm(heights[j-1],heights[j],u));break;}
 f.y=exp(-t*t)*side*(120*lift-6*v.y*ti);out[i]=f;
})";
  NS::Error *error = nullptr;
  auto options = quacknado::metal::adopt(MTL::CompileOptions::alloc()->init());
  options->setFastMathEnabled(false);
  auto library = quacknado::metal::adopt(g.device->newLibrary(
      NS::String::string(source, NS::UTF8StringEncoding), options.get(), &error));
  if (!library)
    throw std::runtime_error(error->localizedDescription()->utf8String());
  auto function = quacknado::metal::adopt(
      library->newFunction(NS::String::string("wind", NS::UTF8StringEncoding)));
  g.pipeline = quacknado::metal::adopt(g.device->newComputePipelineState(function.get(), &error));
  if (!g.pipeline)
    throw std::runtime_error(error->localizedDescription()->utf8String());
  g.input = quacknado::metal::adopt(
      g.device->newBuffer(n * sizeof(bench::Sample), MTL::ResourceStorageModeShared));
  g.output = quacknado::metal::adopt(
      g.device->newBuffer(n * sizeof(simd_float3), MTL::ResourceStorageModeShared));
  if (!g.input || !g.output)
    throw std::runtime_error("Metal buffer allocation failed");
}
GPU::~GPU() = default;
double GPU::run(std::span<const bench::Sample> a, std::span<simd_float3> out) {
  auto pool = quacknado::metal::adopt(NS::AutoreleasePool::alloc()->init());
  auto &g = *impl_;
  std::memcpy(g.input->contents(), a.data(), a.size_bytes());
  auto command = g.queue->commandBuffer();
  auto encoder = command->computeCommandEncoder();
  encoder->setComputePipelineState(g.pipeline.get());
  encoder->setBuffer(g.input.get(), 0, 0);
  encoder->setBuffer(g.output.get(), 0, 1);
  uint32_t n = a.size();
  encoder->setBytes(&n, sizeof n, 2);
  encoder->dispatchThreads(
      MTL::Size(n, 1, 1),
      MTL::Size(std::min<size_t>(256, g.pipeline->maxTotalThreadsPerThreadgroup()), 1, 1));
  encoder->endEncoding();
  command->commit();
  command->waitUntilCompleted();
  if (command->status() == MTL::CommandBufferStatusError)
    throw std::runtime_error("Metal command failed");
  std::memcpy(out.data(), g.output->contents(), out.size_bytes());
  return (command->GPUEndTime() - command->GPUStartTime()) * 1e9;
}
#include <chrono>
#include <cstdio>
void render_benchmark(size_t count, int repetitions) {
  auto pool = quacknado::metal::adopt(NS::AutoreleasePool::alloc()->init());
  auto device = quacknado::metal::adopt(MTL::CreateSystemDefaultDevice());
  if (!device)
    throw std::runtime_error("Metal unavailable");
  auto queue = quacknado::metal::adopt(device->newCommandQueue());
  NS::Error *error = nullptr;
  const char *source = R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 vertex_main(uint v [[vertex_id]],uint instance [[instance_id]],constant uint& base [[buffer(0)]]){
 uint id=base+instance;float2 p=float2(float(id%100)/50-1,float((id/100)%100)/50-1);
 const float2 offsets[]={float2(0,0),float2(.008,0),float2(0,.008)};
 return float4(p+offsets[v],0,1);
}
fragment float4 fragment_main(){return float4(.2,.6,.9,1);})";
  auto lib = quacknado::metal::adopt(
      device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), nullptr, &error));
  if (!lib)
    throw std::runtime_error("Render library failed");
  auto vd = quacknado::metal::adopt(
      lib->newFunction(NS::String::string("vertex_main", NS::UTF8StringEncoding)));
  auto fd = quacknado::metal::adopt(
      lib->newFunction(NS::String::string("fragment_main", NS::UTF8StringEncoding)));
  auto pd = quacknado::metal::adopt(MTL::RenderPipelineDescriptor::alloc()->init());
  pd->setVertexFunction(vd.get());
  pd->setFragmentFunction(fd.get());
  pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
  auto pipeline = quacknado::metal::adopt(device->newRenderPipelineState(pd.get(), &error));
  if (!pipeline)
    throw std::runtime_error("Render pipeline failed");
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
      auto command = queue->commandBuffer();
      auto encoder = command->renderCommandEncoder(pass);
      encoder->setRenderPipelineState(pipeline.get());
      if (mode == 0) {
        for (uint32_t i = 0; i < count; ++i) {
          encoder->setVertexBytes(&i, sizeof i, 0);
          encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        }
      } else {
        uint32_t base = 0;
        encoder->setVertexBytes(&base, sizeof base, 0);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3),
                                NS::UInteger(count));
      }
      encoder->endEncoding();
      command->commit();
      auto submitted = std::chrono::steady_clock::now();
      command->waitUntilCompleted();
      auto end = std::chrono::steady_clock::now();
      if (command->status() == MTL::CommandBufferStatusError)
        throw std::runtime_error("Render command failed");
      texture->getBytes(pixels.data(), 256 * 4, MTL::Region(0, 0, 256, 256), 0);
      if (mode == 0)
        expected = pixels;
      else if (pixels != expected)
        throw std::runtime_error("Instancing image mismatch");
      if (r >= 0) {
        auto ns = [](auto d) { return std::chrono::duration<double, std::nano>(d).count(); };
        std::printf("render_%s,offscreen,%zu,%d,%.0f,%.0f,0\n", mode ? "instanced" : "individual",
                    count, r, ns(end - begin),
                    (command->GPUEndTime() - command->GPUStartTime()) * 1e9);
        std::printf("submit_%s,offscreen,%zu,%d,%.0f,0,0\n", mode ? "instanced" : "individual",
                    count, r, ns(submitted - begin));
      }
    }
}
