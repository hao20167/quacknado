
#include <metal_stdlib>
using namespace metal;

struct Vertex {
  float4 position;
  float4 color;
};

struct Uniforms {
  float angle;
  float aspect;
};

struct VertexOut {
  float4 position [[position]];
  float4 color;
};

vertex VertexOut vertex_main(
  uint id [[vertex_id]],
  const device Vertex* vertices [[buffer(0)]],
  constant Uniforms& u [[buffer(1)]]
) {
  float3 p = vertices[id].position.xyz;

  // y-axis rotation
  float cy = cos(u.angle);
  float sy = sin(u.angle);
  p = float3(cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z);

  // x-axis rotation
  float a = u.angle * 0.77;
  float cx = cos(a);
  float sx = sin(a);
  p = float3(p.x, cx * p.y - sx * p.z, sx * p.y + cx * p.z);

  // move the camera a little
  p.z += 4.0;

  float nearZ = 0.1;
  float farZ = 100.0;
  float f = 1.78320508; // ~58.6 degrees fov

  // transform to clip space
  VertexOut out;
  out.position = float4(p.x * f / u.aspect, p.y * f, farZ * (p.z - nearZ) / (farZ - nearZ), p.z);
  out.color = vertices[id].color;
  return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]]) {
  return in.color;
}
