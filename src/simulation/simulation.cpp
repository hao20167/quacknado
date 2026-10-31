#include "simulation.hpp"

#include <algorithm>
#include <chrono>
#include <random>
#include <stdexcept>

#include <SDL3/SDL.h>

#include "env/tornado.hpp"
#include "simulation/objs/obj_module.hpp"
#include "simulation/physics/physics_world.hpp"
#include "simulation/spawn_position.hpp"

namespace {
float BoundsTop(const IObjModule &object) {
  const auto rotation = simd_matrix3x3(object.orientation_);
  const auto h = object.half_extent_;
  return object.position_.y + std::abs(rotation.columns[0].y) * h.x +
         std::abs(rotation.columns[1].y) * h.y + std::abs(rotation.columns[2].y) * h.z;
}
} // namespace

Simulation::Simulation(uint64_t seed, size_t count, PhysicsOptions physics)
    : physics_(std::make_unique<PhysicsWorld>(physics)), rng_(seed) {
  SDL_Log(" >>> Number of objects: %zu\n", count);
  if (count > kMaxSceneObjects)
    throw std::invalid_argument("Scene count exceeds physics capacity after reserving props");
  bulk_spawn_ = count > 1000;
  objects_.reserve(count + 3);
  auto floor = std::make_unique<IObjModule>();
  floor->position_ = {0, -0.5f, 0};
  floor->half_extent_ = {30, 0.5f, 30};
  floor->affected_by_wind_ = false;
  floor->is_static_ = true;
  floor->body_id_ = physics_->CreateStaticBox(floor->position_, floor->half_extent_);

  objects_.push_back(std::move(floor));

  const auto add_fixed_object = [&](std::unique_ptr<IObjModule> object) {
    object->position_.y = object->half_extent_.y;
    const auto id =
        object->collision_boxes_.empty()
            ? physics_->CreateStaticBox(object->position_, object->half_extent_,
                                        object->orientation_)
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
  for (size_t i = 0; i < count; ++i) {
    const int type = model_distribution(rng_);
    const simd_float3 position{0, 0, 0};
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
      // obj = std::make_unique<Ghast>(position);
      obj = std::make_unique<Sheep>(position);
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
    if (!bulk_spawn_)
      PlaceObject(*obj);
    else
      obj->awaiting_spawn_ = true;
    objects_.push_back(std::move(obj));
  }
  if (bulk_spawn_)
    PlaceBulkScene();
}

bool Simulation::PlaceObject(IObjModule &object, bool prefer_current) {
  object.awaiting_spawn_ = true;
  const auto preferred =
      prefer_current ? std::optional<simd_float3>(object.position_) : std::nullopt;
  const auto position = quacknado::FindSpawnPosition(*physics_, object.half_extent_, rng_,
                                                     object.body_id_, preferred);
  if (!position)
    return false;
  CommitPlacement(object, *position);
  return !object.awaiting_spawn_;
}

void Simulation::CommitPlacement(IObjModule &object, simd_float3 position) {
  if (object.body_id_.IsInvalid()) {
    const auto id =
        object.collision_boxes_.empty()
            ? physics_->CreateDynamicBox(position, object.half_extent_, object.mass_)
            : physics_->CreateDynamicCompound(position, object.collision_boxes_, object.mass_);
    if (id.IsInvalid())
      throw std::runtime_error("Body allocation failed while placing scene");
    object.set_body_id(id);
  } else {
    physics_->ResetBody(object.body_id_, position);
  }
  object.position_ = position;
  object.orientation_ = simd_quaternion(0.0f, 0.0f, 0.0f, 1.0f);
  object.awaiting_spawn_ = false;
}

void Simulation::PlaceBulkScene() {
  float ceiling = .25f;
  std::vector<IObjModule *> pending;
  for (const auto &object : objects_) {
    if (object->awaiting_spawn_)
      pending.push_back(object.get());
    else
      ceiling = std::max(ceiling, BoundsTop(*object));
  }
  // Tall boxes first reduce shelf waste. Stable sort preserves seeded order within a type.
  std::stable_sort(pending.begin(), pending.end(), [](const auto *a, const auto *b) {
    if (a->half_extent_.y != b->half_extent_.y)
      return a->half_extent_.y > b->half_extent_.y;
    return a->half_extent_.z > b->half_extent_.z;
  });
  quacknado::SpawnShelf shelf(ceiling + .4f);
  for (auto *object : pending)
    CommitPlacement(*object, shelf.Next(object->half_extent_));
  physics_->OptimizeBroadPhase();
}

void Simulation::ProcessPendingSpawns() {
  if (bulk_spawn_) {
    simd_float3 max_extent{0, 0, 0};
    for (const auto &object : objects_)
      if (object->awaiting_spawn_ && !object->is_static_)
        max_extent = simd_max(max_extent, object->half_extent_);
    if (max_extent.y == 0)
      return;
    quacknado::RespawnGrid grid(max_extent, respawn_bottom_, respawn_sequence_);
    for (const auto &object : objects_) {
      if (!object->awaiting_spawn_ || object->is_static_)
        continue;
      CommitPlacement(*object, grid.Next(object->half_extent_));
      ++respawned_last_update_;
    }
    return;
  }
  constexpr int max_checks = 8;
  constexpr int max_placements = 4;
  int checks = 0, placements = 0;
  for (size_t visited = 0;
       visited < objects_.size() && checks < max_checks && placements < max_placements; ++visited) {
    auto &object = *objects_[spawn_cursor_];
    spawn_cursor_ = (spawn_cursor_ + 1) % objects_.size();
    if (object.is_static_ || !object.awaiting_spawn_)
      continue;
    ++checks;
    if (PlaceObject(object)) {
      ++placements;
      ++respawned_last_update_;
    }
  }
}

void Simulation::Update(const float &dt) {
  using Clock = std::chrono::steady_clock;
  auto mark = profiling ? Clock::now() : Clock::time_point{};
  auto phase = [&](double &target) {
    if (profiling) {
      auto end = Clock::now();
      target = std::chrono::duration<double, std::nano>(end - mark).count();
      mark = end;
    }
  };
  constexpr float tornado_move_speed = 5.0f;
  tornado_->Move(tornado_direction_ * tornado_move_speed * dt);

  if (wind_enabled_ && wind_workers_) {
    UpdateWindParallel();
  } else if (wind_enabled_) {
    for (const auto &obj : objects_) {
      if (obj->is_static_ || obj->awaiting_spawn_ || obj->body_id_.IsInvalid() ||
          !obj->affected_by_wind_) {
        continue;
      }

      const auto state = physics_->ReadState(obj->body_id_);
      const simd_float3 local_points[]{
          {obj->half_extent_.x, obj->half_extent_.y, obj->half_extent_.z},
          {obj->half_extent_.x, -obj->half_extent_.y, -obj->half_extent_.z},
          {-obj->half_extent_.x, obj->half_extent_.y, -obj->half_extent_.z},
          {-obj->half_extent_.x, -obj->half_extent_.y, obj->half_extent_.z}};

      for (const auto &local_point : local_points) {
        const simd_float3 point = state.position + simd_act(state.orientation, local_point);
        const simd_float3 r = point - state.center_off_mass;
        const simd_float3 point_velocity =
            state.linear_velocity + simd_cross(state.angular_velogity, r);
        const simd_float3 force = ((*tornado_).*force_evaluator_)(point, point_velocity) / 4.0f;

        if (simd_length_squared(force) > 1e-8f) {
          physics_->ApplyForceAt(obj->body_id_, force, point);
        }
      }
    }
  }

  phase(timings.wind_ns);
  physics_->Step(dt);
  phase(timings.physics_ns);

  float spawn_ceiling = .25f;
  respawned_last_update_ = 0;
  for (auto &obj : objects_) {
    if (obj->awaiting_spawn_ || obj->body_id_.IsInvalid())
      continue;
    const auto state = [&] {
      if (pose_only_sync)
        return physics_->ReadPose(obj->body_id_);
      auto full = physics_->ReadState(obj->body_id_);
      return PhysicsWorld::Pose{full.position, full.orientation};
    }();
    obj->position_ = state.position;
    obj->orientation_ = state.orientation;
    if (bulk_spawn_)
      spawn_ceiling = std::max(spawn_ceiling, BoundsTop(*obj));
    if (!obj->is_static_ && state.position.y < -10.0f) {
      physics_->DeactivateBody(obj->body_id_);
      obj->awaiting_spawn_ = true;
    }
  }
  phase(timings.sync_ns);
  if (bulk_spawn_)
    respawn_bottom_ = spawn_ceiling + .4f;
  ProcessPendingSpawns();
  phase(timings.spawn_ns);
}

void Simulation::UpdateWindParallel() {
  wind_work_.clear();
  for (const auto &obj : objects_) {
    if (obj->is_static_ || obj->awaiting_spawn_ || obj->body_id_.IsInvalid() ||
        !obj->affected_by_wind_)
      continue;
    wind_work_.push_back(
        {obj->body_id_, physics_->ReadState(obj->body_id_), obj->half_extent_, {}, {}});
  }
  wind_workers_->run(wind_work_.size(), [this](size_t begin, size_t end) {
    for (size_t i = begin; i < end; ++i) {
      auto &w = wind_work_[i];
      const auto &state = w.state;
      const auto e = w.extent;
      const simd_float3 offsets[] = {
          {e.x, e.y, e.z}, {e.x, -e.y, -e.z}, {-e.x, e.y, -e.z}, {-e.x, -e.y, e.z}};
      for (size_t j = 0; j < 4; ++j) {
        w.points[j] = state.position + simd_act(state.orientation, offsets[j]);
        const auto r = w.points[j] - state.center_off_mass;
        const auto velocity = state.linear_velocity + simd_cross(state.angular_velogity, r);
        w.forces[j] = ((*tornado_).*force_evaluator_)(w.points[j], velocity) / 4.0f;
      }
    }
  });
  // Keep original body/sample order and all Jolt writes on the calling thread.
  for (const auto &w : wind_work_)
    for (size_t j = 0; j < 4; ++j)
      if (simd_length_squared(w.forces[j]) > 1e-8f)
        physics_->ApplyForceAt(w.id, w.forces[j], w.points[j]);
}
