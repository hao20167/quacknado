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
  return float4(in.color.rgb, 1.0);
}


struct ModelVertex {
  float4 position;
  float2 uv;
};

struct ModelOut {
  float4 position [[position]];
  float2 uv;
};

struct ModelMaterial {
  float4 base_color;
  float4 alpha;
};

vertex ModelOut vertex_model(
    uint id [[vertex_id]],
    const device ModelVertex* vertices [[buffer(0)]],
    constant Uniforms& u [[buffer(1)]]) {
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
  float f = 1.78320508;
  float aspect = u.offset_aspect.w;

  ModelOut out;
  out.position = float4(p.x * f / aspect, p.y * f, farZ * (p.z - nearZ) / (farZ - nearZ), p.z);
  out.uv = vertices[id].uv;
  return out;
}

fragment float4 fragment_model(
    ModelOut in [[stage_in]],
    texture2d<float> base_color [[texture(0)]],
    constant ModelMaterial& material [[buffer(0)]]) {
  constexpr sampler pixel_sampler(coord::normalized, address::repeat,
                                  min_filter::nearest, mag_filter::nearest,
                                  mip_filter::nearest);
  float4 color = base_color.sample(pixel_sampler, in.uv) * material.base_color;
  if (material.alpha.x >= 0 && color.a < material.alpha.x) discard_fragment();
  return float4(color.rgb, 1);
}

struct TornadoUniforms {
  float4 center_radius;
  float4 aspect_phase;
};

struct TornadoOut {
  float4 position [[position]];
  float2 local;
};

vertex TornadoOut vertex_tornado(uint id [[vertex_id]],
                                constant TornadoUniforms& u [[buffer(0)]]) {
  constexpr float2 corners[] = {
    float2(-1, -1), float2(1, -1), float2(1, 1),
    float2(-1, -1), float2(1, 1), float2(-1, 1)
  };
  float2 local = corners[id] * (u.center_radius.w * 1.28);
  float3 world = u.center_radius.xyz + float3(local.x, 0, local.y);
  float3 eye = float3(0, 14, -40);
  float3 forward = normalize(float3(0, 6, 0) - eye);
  float3 right = normalize(cross(float3(0, 1, 0), forward));
  float3 up = cross(forward, right);
  float3 relative = world - eye;
  float3 p = float3(dot(relative, right), dot(relative, up), dot(relative, forward));
  TornadoOut out;
  out.position = float4(p.x * 1.78320508 / u.aspect_phase.x, p.y * 1.78320508,
                       200.0 * (p.z - 0.1) / (200.0 - 0.1), p.z);
  out.local = local;
  return out;
}


float2 MagicRotate(float2 p, float angle) {
  float c = cos(angle), s = sin(angle);
  return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float MagicSegment(float2 p, float2 a, float2 b) {
  float2 ab = b - a;
  float t = clamp(dot(p - a, ab) / dot(ab, ab), 0.0, 1.0);
  return length(p - a - ab * t);
}

float MagicRune(float2 p, int id) {
  if (id % 2 == 1) p.x = -p.x;
  float d = MagicSegment(p, float2(0, -0.034), float2(0, 0.034));
  d = min(d, MagicSegment(p, float2(0, 0.025), float2(0.026, 0.004)));
  if (id % 3 == 0) {
    d = min(d, MagicSegment(p, float2(0.026, 0.004), float2(0, -0.015)));
    d = min(d, MagicSegment(p, float2(-0.024, 0.014), float2(0, -0.007)));
  } else if (id % 3 == 1) {
    d = min(d, MagicSegment(p, float2(-0.022, -0.009), float2(0.022, -0.009)));
    d = min(d, MagicSegment(p, float2(-0.022, 0.034), float2(0, 0.012)));
  } else {
    d = min(d, MagicSegment(p, float2(0, 0.007), float2(0.026, -0.014)));
    d = min(d, MagicSegment(p, float2(0, -0.034), float2(-0.022, -0.016)));
  }
  return d;
}

float MagicHash(float2 p) {
  return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float MagicNoise(float2 p) {
  float2 cell = floor(p), f = fract(p);
  float2 blend = f * f * (3.0 - 2.0 * f);
  return mix(mix(MagicHash(cell), MagicHash(cell + float2(1, 0)), blend.x),
             mix(MagicHash(cell + float2(0, 1)), MagicHash(cell + float2(1, 1)), blend.x), blend.y);
}

fragment float4 fragment_tornado(TornadoOut in [[stage_in]],
                                constant TornadoUniforms& u [[buffer(0)]]) {
  constexpr float tau = 2.0 * M_PI_F;
  float2 p = in.local / max(u.center_radius.w, 0.001);
  float r = length(p);
  float phase = u.aspect_phase.y;
  float angle = atan2(p.y, p.x);
  float aa = max(fwidth(r) * 0.6, 0.0008);
  float2 drift = float2(cos(phase * 8.0), sin(phase * 8.0));
  float noise = MagicNoise(p * 35.0 + drift * 2.0) * 0.6
              + MagicNoise(p * 85.0 - drift * 4.0) * 0.4;
  float fire_radius = 1.0 + (noise - 0.5) * 0.014;
  float distance = min(abs(r - fire_radius), abs(r - 0.945));
  distance = min(distance, abs(r - 0.82));
  distance = min(distance, abs(r - 0.635));

  float2 rune_position = MagicRotate(p, -phase);
  float rune_angle = atan2(rune_position.y, rune_position.x);
  float rune_sector = floor(rune_angle * (32.0 / tau) + 0.5);
  float local_angle = rune_angle - rune_sector * (tau / 32.0);
  float2 glyph = float2(local_angle * 0.88, r - 0.882);
  float glyph_distance = MagicRune(glyph, (int(rune_sector) + 32) % 32);
  if (int(rune_sector + 32) % 4 == 0)
    glyph_distance = min(glyph_distance, abs(length(glyph - float2(-0.016, 0.015)) - 0.012));
  distance = min(distance, glyph_distance);

  float inner_angle = angle + phase * 2.0;
  float inner_sector = floor(inner_angle * (40.0 / tau) + 0.5);
  float2 inner_glyph = float2((inner_angle - inner_sector * tau / 40.0) * 0.72, r - 0.72) * 1.6;
  distance = min(distance, MagicRune(inner_glyph, (int(inner_sector) + 80) % 40) / 1.6);
  distance = min(distance, abs(r - 0.675));
  distance = min(distance, abs(r - 0.765));

  float2 seal = MagicRotate(p, phase * 2.0);
  for (int i = 0; i < 3; ++i) {
    float a = tau * float(i) / 3.0 + M_PI_F * 0.5;
    float b = a + tau / 3.0;
    float2 v0 = float2(cos(a), sin(a)) * 0.6;
    float2 v1 = float2(cos(b), sin(b)) * 0.6;
    distance = min(distance, MagicSegment(seal, v0, v1));
    distance = min(distance, MagicSegment(-seal, v0, v1));
  }

  float arc = fract((angle - phase * 3.0) * (3.0 / tau));
  float inner_distance = 10.0;
  if (arc > 0.10 && arc < 0.94) inner_distance = abs(r - 0.285);
  float inner_core = 1.0 - smoothstep(0.017, 0.017 + aa, inner_distance);
  distance = min(distance, max(inner_distance - 0.012, 0.0));
  float core = 1.0 - smoothstep(0.0045, 0.0045 + aa, distance);
  float tight_glow = exp(-distance * 85.0) * 0.70;
  float wide_glow = exp(-distance * 20.0) * 0.24;
  float pulse = 0.92 + 0.08 * sin(phase * 8.0);

  float2 swirl = MagicRotate(p, phase * 5.0 + r * 9.0);
  float fire = MagicNoise(swirl * 28.0 + drift * 3.0) * 0.65
             + MagicNoise(swirl * 60.0 - drift * 2.0) * 0.35;
  float center_radius = 0.147 + (fire - 0.5) * 0.025;
  float center = (1.0 - smoothstep(center_radius, center_radius + aa, r))
               * smoothstep(0.026, 0.026 + aa, r);
  float center_glow = exp(-abs(r - 0.11) * 20.0) * 0.22;

  float spark_id = floor((angle + M_PI_F) * (96.0 / tau));
  float seed = MagicHash(float2(spark_id, 17));
  float progress = fract(phase * (8.0 / tau) + seed);
  float spark_angle = (spark_id + 0.5) * (tau / 96.0) - M_PI_F + progress * 0.012;
  float2 spark_position = MagicRotate(p, -spark_angle) - float2(1.015 + progress * 0.22, 0);
  float spark_distance = length(spark_position * float2(0.6, 1.0));
  float sparks = (1.0 - smoothstep(0.0015 + seed * 0.002, 0.0025 + seed * 0.002 + aa, spark_distance))
               * pow(1.0 - progress, 1.5);
  float spark_glow = exp(-spark_distance * 200.0) * pow(1.0 - progress, 2.0) * 0.22;

  float aura = exp(-abs(r - 1.0) * 14.0) * 0.18 + exp(-r * r * 2.5) * 0.04;
  float fade = 1.0 - smoothstep(1.22, 1.28, r);
  float coverage = clamp(max(max(core, inner_core), max(center, sparks)) * 0.98, 0.0, 0.98) * fade;
  float heat = clamp(inner_core * 0.22 + center * fire * 0.72 + sparks * 0.35, 0.0, 1.0);
  float3 hot_color = mix(float3(1.0, 0.22, 0.006), float3(1.0, 0.72, 0.12), heat);
  float3 halo = float3(1.0, 0.025, 0.001) * (wide_glow + center_glow + spark_glow + aura) * 0.55
              + float3(1.0, 0.13, 0.003) * tight_glow * 0.24;
  float3 energy = hot_color * coverage * pulse + halo * fade * (1.0 - coverage);
  if (max(energy.r, max(energy.g, energy.b)) < 0.003) discard_fragment();
  float veil = (1.0 - smoothstep(0.96, 1.28, r)) * 0.95;
  float opacity = coverage + veil * (1.0 - coverage);
  return float4(energy, opacity);
}
