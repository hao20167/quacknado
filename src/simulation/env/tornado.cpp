#include "tornado.hpp"

#include <algorithm>
#include <cmath>

namespace {

float Smoothstep(float low, float high, float value) {
  float t = std::clamp((value - low) / (high - low), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}

simd_float3 Tornado::VelocityAt(const simd_float3& position) const {
  float h = position.y - center_.y;
  if (height_ <= 0.0f || h < 0.0f || h >= height_) return {};

  float u = h / height_;
  float cur_radius = bottom_radius_ + (top_radius_ - bottom_radius_) * u;
  if (cur_radius <= 0.0f) return {};

  float dx = position.x - center_.x;
  float dz = position.z - center_.z;
  float r = std::sqrt(dx * dx + dz * dz);
  float t = r / cur_radius;
  if (t >= 1.5f) return {};

  float inv_r = r > 1e-5f ? 1.0f / r : 0.0f;
  simd_float3 radial {dx * inv_r, 0, dz * inv_r};
  simd_float3 tangent {-radial.z, 0, radial.x};

  float top_blend = Smoothstep(0.7f, 0.95f, u);
  float radial_speed = -suction_speed_ * (1.0f - top_blend) + outward_speed_ * top_blend;
  float swirl_falloff = t <= 1.0f ? t : 1.0f / t;
  float ground_lift = 0.25f + 0.75f * Smoothstep(0.0f, 4.0f, h);

  simd_float3 rotation = tangent * tangential_speed_ * swirl_falloff * (1.0f - 0.5f * top_blend);
  simd_float3 flow = radial * radial_speed * std::min(t, 1.0f);
  simd_float3 lift {0, lift_speed_ * ground_lift * std::exp(-t * t) * (1.0f - top_blend), 0};

  return rotation + flow + lift;
}

simd_float3 Tornado::ForceAt(const simd_float3& position, const simd_float3& velocity) const {
  float h = position.y - center_.y;
  if (height_ <= 0.0f || h < 0.0f || h >= height_) return {};

  float u = h / height_;
  float cur_radius = bottom_radius_ + (top_radius_ - bottom_radius_) * u;
  if (cur_radius <= 0.0f) return {};

  float dx = position.x - center_.x;
  float dz = position.z - center_.z;
  float t = std::sqrt(dx * dx + dz * dz) / cur_radius;
  float side_influence = 1.0f - Smoothstep(1.0f, 1.5f, t);
  float top_influence = 1.0f - Smoothstep(0.95f, 1.0f, u);
  float influence = side_influence * top_influence;

  constexpr float drag = 3.0f;
  return (VelocityAt(position) - velocity) * drag * influence;
}
