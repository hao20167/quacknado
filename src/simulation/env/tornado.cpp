#include "tornado.hpp"

#include <cmath>
#include <simd/quaternion.h>
#include <simd/simd.h>

#include "simulation/objs/obj_module.hpp"


simd_float3 Tornado::VelocityAt(const simd_float3& position) const {
  float dx = position.x - center_.x;
  float dz = position.z - center_.z;
  float r = std::sqrt(dx * dx + dz * dz);

  if (radius_ <= 0.0f || r >= radius_) return {};

  float inv_r = r > 1e-5f ? 1.0f / r : 0.0f;
  simd_float3 radial {dx * inv_r, 0, dz * inv_r};
  simd_float3 tangent {-radial.z, 0, radial.x};
  float falloff = 1.0f - r / radius_;
  simd_float3 rotation = tangent * tangential_speed_;
  simd_float3 suction = radial * (-suction_speed_);
  simd_float3 lift {0, lift_speed_, 0};

  return (rotation + suction + lift) * falloff;
}

simd_float3 Tornado::ForceAt(const simd_float3& position, const simd_float3& velocity, const float& mass) const {
  simd_float3 wind_velocity = VelocityAt(position);
  simd_float3 relative_velocity = wind_velocity - velocity;
  return relative_velocity * (mass * 3.0f);
}

void Tornado::UpdateObj(IObjModule* obj, const float& dt) {
  simd_float3 local_point = {obj->size_ * 0.25f, obj->size_ * 0.25f, 0.0f};
  simd_float3 r = simd_act(obj->orientation_, local_point);
  simd_float3 world_point = obj->position_ + r;
  simd_float3 point_velocity = obj->velocity_ + simd_cross(obj->angular_velocity_, r);
  simd_float3 wind_velocity = VelocityAt(world_point);

  simd_float3 force {};
  float wind_strength = 3.0f;

  float dx = world_point.x - center_.x;
  float dz = world_point.z - center_.z;
  if (radius_ > 0.0f && dx * dx + dz * dz <= radius_ * radius_) {
    simd_float3 relative_velocity = wind_velocity - point_velocity;
    force = relative_velocity * wind_strength;
  }

  simd_float3 torque = simd_cross(r, force);

  simd_float3 acceleration = force / obj->mass_;
  acceleration.y -= 9.81f;
  obj->velocity_ += acceleration * dt;
  obj->position_ += obj->velocity_ * dt;

  float inertia = obj->mass_ * obj->size_ * obj->size_ / 6.0f; 
  simd_float3 angular_accleration = torque / inertia;
  obj->angular_velocity_ += angular_accleration * dt;

  // approx
  // auto& w = obj->angular_velocity_;
  // simd_quatf omega = simd_quaternion(w.x, w.y, w.z, 0.0f);
  // simd_quatf qdot = simd_mul(omega, obj->orientation_);
  // obj->orientation_ = simd_normalize(simd_add(obj->orientation_, simd_mul(qdot, 0.5f * dt)));
  
  // exact
  float speed = simd_length(obj->angular_velocity_);
  if (speed > 1e-6f) {
    simd_float3 axis = obj->angular_velocity_ / speed;
    simd_quatf delta = simd_quaternion(speed * dt, axis);
    obj->orientation_ = simd_normalize(simd_mul(delta, obj->orientation_));
  }
}
