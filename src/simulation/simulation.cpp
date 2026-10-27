#include "simulation.hpp"

#include <cmath>
#include <numbers>
#include <random>

#include "env/tornado.hpp"
#include "objs/duck.hpp"
#include "simulation/objs/obj_module.hpp"

Simulation::Simulation() {
  auto floor = std::make_unique<IObjModule>();
  floor->position_ = {0, -0.5f, 0};
  floor->half_extent_ = {30, 0.5f, 30};
  floor->affected_by_wind_ = false;
  floor->body_id_ = physics_->CreateStaticBox(floor->position_, floor->half_extent_);

  objects_.push_back(std::move(floor));

  std::mt19937_64 rng(std::random_device{}());
  std::uniform_real_distribution<float> angle_distribution(0, 2.0f * std::numbers::pi_v<float>);
  std::uniform_real_distribution<float> radius_distribution(5.0f * 5.0f, 15.0f * 15.0f);
  std::uniform_real_distribution<float> height_distribution(1.0f, 8.0f);

  std::vector<simd_float3> spawn_positions;
  spawn_positions.reserve(400);
  for (int i = 0; i < 200; i++) {
    float angle = angle_distribution(rng);
    float radius = std::sqrt(radius_distribution(rng));
    spawn_positions.push_back(simd_float3{radius * 1.5f * std::cos(angle), height_distribution(rng), radius * std::sin(angle)});
  }

  for (auto position: spawn_positions) {
    auto duck = std::make_unique<Duck>();
    duck->position_ = position;
    duck->body_id_ = physics_->CreateDynamicBox(duck->position_, duck->half_extent_, duck->mass_);

    objects_.push_back(std::move(duck));
  }
}

void Simulation::Update(const float& dt) {
  if (wind_enabled_) {
    for (const auto& obj: objects_) {
      if (!obj->affected_by_wind_) {
        continue;
      }

      const auto state = physics_->ReadState(obj->body_id_);
      const simd_float3 local_points[] {
        { obj->half_extent_.x,  obj->half_extent_.y,  obj->half_extent_.z},
        { obj->half_extent_.x, -obj->half_extent_.y, -obj->half_extent_.z},
        {-obj->half_extent_.x,  obj->half_extent_.y, -obj->half_extent_.z},
        {-obj->half_extent_.x, -obj->half_extent_.y,  obj->half_extent_.z}
      };

      for (const auto& local_point: local_points) {
        const simd_float3 r = simd_act(state.orientation, local_point);
        const simd_float3 point = state.center_off_mass + r;
        const simd_float3 point_velocity = state.linear_velocity + simd_cross(state.angular_velogity, r);
        const simd_float3 force = tornado_->ForceAt(point, point_velocity) / 4.0f;

        if (simd_length_squared(force) > 1e-8f) {
          physics_->ApplyForceAt(obj->body_id_, force, point);
        }
      }
    }
  }

  physics_->Step(dt);

  for (auto& obj: objects_) {
    auto state = physics_->ReadState(obj->body_id_);
    if (wind_enabled_ && obj->affected_by_wind_ && state.position.y < -10.0f) {
      static std::mt19937_64 rng(std::random_device{}());
      std::uniform_real_distribution<float> angle_distribution(0, 2.0f * std::numbers::pi_v<float>);
      std::uniform_real_distribution<float> radius_distribution(5.0f, 12.0f);
      float angle = angle_distribution(rng);
      float radius = radius_distribution(rng);
      physics_->ResetBody(obj->body_id_, {radius * std::cos(angle), 1.5f, radius * std::sin(angle)});
      state = physics_->ReadState(obj->body_id_);
    }
    obj->position_ = state.position;
    obj->orientation_ = state.orientation;
  }
}
