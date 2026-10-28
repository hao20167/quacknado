#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <simd/simd.h>
#include <Metal/Metal.hpp>

namespace quacknado {
struct ModelVertex {
  simd_float4 position;
  simd_float2 uv;
};
static_assert(sizeof(ModelVertex) == 32);

struct ModelPrimitive {
  std::vector<ModelVertex> vertices;
  std::vector<uint32_t> indices;
  NS::SharedPtr<MTL::Texture> texture;
  simd_float4 base_color {1, 1, 1, 1};
  float alpha_cutoff = -1;
};
struct GlbAsset {
  std::vector<ModelPrimitive> primitives;
};

GlbAsset LoadGlb(const std::string& path, MTL::Device* device);
}
