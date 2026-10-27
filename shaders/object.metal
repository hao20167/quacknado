#include <metal_stdlib>
using namespace metal;

struct Vertex {
  float4 position;
  float4 color;
};

struct Uniforms {
  float4 orientation;
  float4 offset_aspect;
  float4 half_extent;
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
  float3 world = vertices[id].position.xyz * u.half_extent.xyz;
  world = RotateByQuaternion(world, u.orientation);
  world += u.offset_aspect.xyz;

  float3 eye = float3(0, 14, -40);
  float3 target = float3(0, 6, 0);
  float3 forward = normalize(target - eye);
  float3 right = normalize(cross(float3(0, 1, 0), forward));
  float3 up = cross(forward, right);
  float3 relative = world - eye;
  
  float3 p = float3(dot(relative, right), dot(relative, up), dot(relative, forward));

  float nearZ = 0.1;
  float farZ = 200.0;
  float f = 1.78320508; // ~58.6 degrees fov
  float aspect = u.offset_aspect.w;

  // transform to clip space
  VertexOut out;
  out.position = float4(p.x * f / aspect, p.y * f, farZ * (p.z - nearZ) / (farZ - nearZ), p.z);
  out.color = vertices[id].color;
  return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]]) {
  return in.color;
}
