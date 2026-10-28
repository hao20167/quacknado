#pragma once

#include <memory>
#include <random>

#include "physics/physics_world.hpp"
#include "env/tornado.hpp"
#include "objs/obj_module.hpp"

class Simulation {
private:
  std::unique_ptr<PhysicsWorld> physics_ = std::make_unique<PhysicsWorld>();
  std::unique_ptr<Tornado> tornado_ = std::make_unique<Tornado>();
  std::vector<std::unique_ptr<IObjModule>> objects_;
  bool wind_enabled_ = false;
  std::mt19937_64 rng_;
  size_t spawn_cursor_ = 0;
  bool PlaceObject(IObjModule& object, bool prefer_current = false);
  void ProcessPendingSpawns();
public:
  explicit Simulation(uint64_t seed = std::random_device{}());

  const auto& get_objects() const {
    return objects_;
  }

  void set_wind_enabled(bool enabled) {
    wind_enabled_ = enabled;
  }

  void Update(const float& dt);
};
