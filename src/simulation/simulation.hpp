#pragma once

#include "core/worker_pool.hpp"
#include <array>
#include <memory>
#include <random>

#include "env/tornado.hpp"
#include "objs/obj_module.hpp"
#include "physics/physics_world.hpp"
#include "spawn_position.hpp"

class Simulation {
private:
  std::unique_ptr<PhysicsWorld> physics_;
  std::unique_ptr<Tornado> tornado_ = std::make_unique<Tornado>();
  std::vector<std::unique_ptr<IObjModule>> objects_;
  using ForceEvaluator = simd_float3 (Tornado::*)(const simd_float3 &, const simd_float3 &) const;
  ForceEvaluator force_evaluator_ = &Tornado::ForceAt;
  struct WindWork {
    JPH::BodyID id;
    BodyState state;
    simd_float3 extent;
    std::array<simd_float3, 4> points, forces;
  };
  std::vector<WindWork> wind_work_;
  std::unique_ptr<quacknado::WorkerPool> wind_workers_;
  void UpdateWindParallel();
  bool wind_enabled_ = false;
  simd_float3 tornado_direction_{0, 0, 0};
  std::mt19937_64 rng_;
  size_t spawn_cursor_ = 0;
  bool bulk_spawn_ = false;
  uint64_t respawn_sequence_ = 0;
  float respawn_bottom_ = 0;
  size_t respawned_last_update_ = 0;
  void PlaceBulkScene();
  void CommitPlacement(IObjModule &object, simd_float3 position);

  bool PlaceObject(IObjModule &object, bool prefer_current = false);
  void ProcessPendingSpawns();

public:
  explicit Simulation(uint64_t seed = std::random_device{}(), size_t count = 10000,
                      PhysicsOptions physics = {});

  struct Timings {
    double wind_ns = 0, physics_ns = 0, sync_ns = 0, spawn_ns = 0;
  };
  Timings timings;
  bool profiling = false;
  ContactCounts contacts() const { return physics_->Contacts(); }
  bool pose_only_sync = true;
  void set_physics_access(bool coalesced, bool redundant_check) {
    physics_->coalesced_state_reads = coalesced;
    physics_->redundant_force_check = redundant_check;
  }
  void set_wind_workers(size_t count) {
    wind_workers_ = count ? std::make_unique<quacknado::WorkerPool>(count) : nullptr;
    wind_work_.reserve(objects_.size());
  }
  std::vector<BodyState> body_states() const {
    std::vector<BodyState> states;
    for (const auto &o : objects_)
      if (!o->awaiting_spawn_ && !o->body_id_.IsInvalid())
        states.push_back(physics_->ReadState(o->body_id_));
    return states;
  }
  size_t respawned_last_update() const { return respawned_last_update_; }
  unsigned active_body_count() const { return physics_->ActiveBodyCount(); }
  void set_force_evaluator(ForceEvaluator evaluator) { force_evaluator_ = evaluator; }
  const auto &get_objects() const { return objects_; }

  const Tornado &get_tornado() const { return *tornado_; }

  void set_wind_enabled(bool enabled) { wind_enabled_ = enabled; }

  void set_tornado_direction(simd_float3 direction) {
    direction.y = 0;
    tornado_direction_ =
        simd_length_squared(direction) > 1.0f ? simd_normalize(direction) : direction;
  }

  void Update(const float &dt);
};
