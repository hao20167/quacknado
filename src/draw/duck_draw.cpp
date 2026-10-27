#include "draw/duck_draw.hpp"

#include <vector>

#include <Metal/Metal.hpp>
#include <Foundation/Foundation.hpp>
#include <SDL3/SDL.h>

struct Vertex {
  float pos[4];
  float color[4];
};

struct Uniforms { // TODO: alignas, static_assert
  simd_float4 orientation;
  simd_float3 offset;
  float aspect;
};

DuckDraw::DuckDraw(Duck* ptr) {
  duck_ = ptr;
}

DuckDraw::~DuckDraw() = default;

std::vector<Vertex> MakeDuck() {
  static const float p[8][4] = {
    {-1,-1,-1,1}, { 1,-1,-1,1},
    { 1, 1,-1,1}, {-1, 1,-1,1},
    {-1,-1, 1,1}, { 1,-1, 1,1},
    { 1, 1, 1,1}, {-1, 1, 1,1}
  };

  static const int faces[6][4] = {
    {4,5,6,7}, {1,0,3,2},
    {0,4,7,3}, {5,1,2,6},
    {3,7,6,2}, {0,1,5,4}
  };

  static const float colors[6][4] = {
    {1,0,0,1}, {0,1,0,1},
    {0,0,1,1}, {1,1,0,1},
    {1,0,1,1}, {0,1,1,1}
  };

  std::vector<Vertex> result;
  result.reserve(36);

  static const int indices[6] = {0, 1, 2, 0, 2, 3};

  for (int f = 0; f < 6; f++) {
    for (int i: indices) {
      Vertex v{};
      int n = faces[f][i];
      for (int j = 0; j < 4; j++) {
        v.pos[j] = p[n][j];
        v.color[j] = colors[f][j];
      }

      result.push_back(v);
    }
  }
  return result;
}

bool DuckDraw::Initialize(MTL::Device* device, MTL::Library* library, MTL::PixelFormat color_format) {
  if (device == nullptr || library == nullptr || pipeline_.get()) {
    return false;
  }
  auto vertex_function = NS::TransferPtr(library->newFunction(
    NS::String::string("vertex_main", NS::UTF8StringEncoding)
  ));
  auto fragment_function = NS::TransferPtr(library->newFunction(
    NS::String::string("fragment_main", NS::UTF8StringEncoding)
  ));
  if (!vertex_function.get() || !fragment_function.get()) {
    SDL_Log("Duck shader functions cannot be initialized.\n");
    return false;
  }

  auto descriptor = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
  descriptor->setVertexFunction(vertex_function.get());
  descriptor->setFragmentFunction(fragment_function.get());
  descriptor->colorAttachments()->object(0)->setPixelFormat(color_format);
  descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
  NS::Error* error = nullptr;
  pipeline_ = NS::TransferPtr(device->newRenderPipelineState(descriptor.get(), &error));
  if (!pipeline_.get()) {
    SDL_Log("Duck Pipeline failed.\n");
    if (error) {
      SDL_Log("%s\n", error->localizedDescription()->utf8String());
    }
    return false;
  }

  start_ = SDL_GetTicks();
  auto vertices = MakeDuck();
  buffer_ = NS::TransferPtr(device->newBuffer(vertices.data(), vertices.size() * sizeof(Vertex), MTL::ResourceStorageModeShared));

  return true;
}

void DuckDraw::Record(MTL::RenderCommandEncoder* encoder, int width, int height) {
  if (!encoder || !pipeline_.get()) {
    return;
  }

  Uniforms uniforms {
    duck_->orientation_.vector,
    duck_->position_,
    float(width) / float(height)
  };

  encoder->setRenderPipelineState(pipeline_.get());
  encoder->setCullMode(MTL::CullModeNone);
  encoder->setVertexBuffer(buffer_.get(), NS::UInteger(0), 0);
  encoder->setVertexBytes(&uniforms, sizeof uniforms, 1);
  encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), 36);
}
