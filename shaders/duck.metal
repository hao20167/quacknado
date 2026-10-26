#include <metal_stdlib>
using namespace metal;

struct Vertex {
  float4 position;
  float4 color;
};

struct Uniforms {
  float4 orientation;
  float3 offset;
  float aspect;
};

struct VertexOut {
  float4 position [[position]];
  float4 color;
};

float3 RotateByQuaternion(float3 p, float4 q) {
  float3 t = 2.0f * cross(q.xyz, p);
  return p + q.w * t + cross(q.xyz, t);
}

vertex VertexOut vertex_main(
  uint id [[vertex_id]],
  const device Vertex* vertices [[buffer(0)]],
  constant Uniforms& u [[buffer(1)]]
) {
  float3 p = vertices[id].position.xyz;

  p = RotateByQuaternion(p, u.orientation);

  p += u.offset;
  // move the camera a little
  p.z += 50.0;

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
