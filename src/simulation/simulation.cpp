#include "simulation.hpp"

#include <random>

#include "env/tornado.hpp"
#include "simulation/spawn_position.hpp"
#include "simulation/objs/obj_module.hpp"
#include "simulation/physics/physics_world.hpp"

Simulation::Simulation(uint64_t seed) : rng_(seed) {
  auto floor = std::make_unique<IObjModule>();
  floor->position_ = {0, -0.5f, 0};
  floor->half_extent_ = {30, 0.5f, 30};
  floor->affected_by_wind_ = false;
  floor->is_static_ = true;
  floor->body_id_ = physics_->CreateStaticBox(floor->position_, floor->half_extent_);

  objects_.push_back(std::move(floor));

  const auto add_fixed_object = [&](std::unique_ptr<IObjModule> object) {
    object->position_.y = object->half_extent_.y;
    const auto id = object->collision_boxes_.empty()
        ? physics_->CreateStaticBox(object->position_, object->half_extent_, object->orientation_)
        : physics_->CreateStaticCompound(object->position_, object->collision_boxes_);
    if (!id.IsInvalid()) {
      object->set_body_id(id);
      objects_.push_back(std::move(object));
    }
  };
  // add_fixed_object(std::make_unique<House>(simd_float3{-10, 0, -12}));
  add_fixed_object(std::make_unique<PChair>(simd_float3{0, 0, -8}));

  auto dirt_house = std::make_unique<DirtHouse>(simd_float3{8, 0, -6});
  dirt_house->position_.y = dirt_house->half_extent_.y + 0.25f;
  PlaceObject(*dirt_house, true);
  objects_.push_back(std::move(dirt_house));

  std::uniform_int_distribution<int> model_distribution(1, 9);
  for (int i = 0; i < 200; ++i) {
    const int type = model_distribution(rng_);
    const simd_float3 position {0, 0, 0};
    std::unique_ptr<IObjModule> obj;
    switch (type) {
      case 1:
        obj = std::make_unique<Chicken>(position);
        break;
      case 2:
        obj = std::make_unique<Boat>(position);
        break;
      case 3:
        obj = std::make_unique<DirtBlock>(position);
        break;
      case 4:
        obj = std::make_unique<Ghast>(position);
        break;
      case 5:
        obj = std::make_unique<Sheep>(position);
        break;
      case 6:
        obj = std::make_unique<Villager>(position);
        break;
      case 7:
        obj = std::make_unique<Chest>(position);
        break;
      case 8:
        obj = std::make_unique<Creeper>(position);
        break;
      case 9:
        obj = std::make_unique<Steve>(position);
        break;
    }
    PlaceObject(*obj);
    objects_.push_back(std::move(obj));
  }
}

bool Simulation::PlaceObject(IObjModule& object, bool prefer_current) {
  object.awaiting_spawn_ = true;
  const auto preferred = prefer_current ? std::optional<simd_float3>(object.position_) : std::nullopt;
  const auto position = quacknado::FindSpawnPosition(*physics_, object.half_extent_, rng_, object.body_id_, preferred);
  if (!position) return false;
  if (object.body_id_.IsInvalid()) {
    const auto id = object.collision_boxes_.empty()
        ? physics_->CreateDynamicBox(*position, object.half_extent_, object.mass_)
        : physics_->CreateDynamicCompound(*position, object.collision_boxes_, object.mass_);
    if (id.IsInvalid()) return false;
    object.set_body_id(id);
  } else {
    physics_->ResetBody(object.body_id_, *position);
  }
  object.position_ = *position;
  object.orientation_ = simd_quaternion(0.0f, 0.0f, 0.0f, 1.0f);
  object.awaiting_spawn_ = false;
  return true;
}

void Simulation::ProcessPendingSpawns() {
  constexpr int max_checks = 8;
  constexpr int max_placements = 4;
  int checks = 0, placements = 0;
  for (size_t visited = 0; visited < objects_.size() && checks < max_checks && placements < max_placements; ++visited) {
    auto& object = *objects_[spawn_cursor_];
    spawn_cursor_ = (spawn_cursor_ + 1) % objects_.size();
    if (object.is_static_ || !object.awaiting_spawn_) continue;
    ++checks;
    if (PlaceObject(object)) ++placements;
  }
}

void Simulation::Update(const float& dt) {
  if (wind_enabled_) {
    for (const auto& obj: objects_) {
      if (obj->is_static_ || obj->awaiting_spawn_ || obj->body_id_.IsInvalid() || !obj->affected_by_wind_) {
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
        const simd_float3 point = state.position + simd_act(state.orientation, local_point);
        const simd_float3 r = point - state.center_off_mass;
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
    if (obj->awaiting_spawn_ || obj->body_id_.IsInvalid()) continue;
    const auto state = physics_->ReadState(obj->body_id_);
    obj->position_ = state.position;
    obj->orientation_ = state.orientation;
    if (!obj->is_static_ && state.position.y < -10.0f) {
      physics_->DeactivateBody(obj->body_id_);
      obj->awaiting_spawn_ = true;
    }
  }
  ProcessPendingSpawns();
}
