#include <metal_stdlib>
using namespace metal;

struct VertexOut {
  float4 position [[position]];
  float4 color;
};

vertex VertexOut vertexMain(
  uint vertexId [[vertex_id]],
  constant float2& offset [[buffer(0)]]
) {
  const float2 vertices[3] = {
    float2(0.0, 0.15),
    float2(-0.15, -0.15),
    float2(0.15, -0.15)
  };

  VertexOut out;
  float2 postition = vertices[vertexId] + offset;
  out.position = float4(postition, 0.0, 1.0);
  out.color = float4(0.2, 0.8, 1.0, 1.0);

  return out;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]]) {
  return in.color;
}
