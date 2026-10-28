#pragma once

#include <string>
#include <vector>
#include <simd/simd.h>

#include "simulation/physics/physics_world.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

class IObjModule {
public:
  std::string name_;
  JPH::BodyID body_id_;
  simd_float3 position_;
  simd_quatf orientation_ = simd_quaternion(0.0f, 0.0f, 0.0f, 1.0f);
  simd_float3 half_extent_;
  float visual_height_;
  float mass_;
  bool affected_by_wind_ = true;
  bool is_static_ = false;
  bool awaiting_spawn_ = false;
  std::vector<CollisionBox> collision_boxes_;

  virtual ~IObjModule() = default;

  void set_body_id(const JPH::BodyID& body_id) {
    body_id_ = body_id;
  }

  void set_affected_by_wind(const bool& affected_by_wind) {
    affected_by_wind_ = affected_by_wind;
  }
};

class Chicken: public IObjModule {
public:
  Chicken(const simd_float3& position) {
    name_ = "chicken";
    position_ = position;
    half_extent_ = simd_float3{0.3f, 0.6f, 0.45f};
    visual_height_ = 1.2f;
    mass_ = 1.0f;
  }
};


class Creeper: public IObjModule {
public:
  Creeper(const simd_float3& position) {
    name_ = "creeper";
    position_ = position;
    half_extent_ = simd_float3{0.4f, 1.3f, 0.6f};
    visual_height_ = 2.6f;
    mass_ = 2.0f;
  }
};

class Boat: public IObjModule {
public:
  Boat(const simd_float3& position) {
    name_ = "boat";
    position_ = position;
    half_extent_ = simd_float3{0.88f, 0.4f, 1.12f};
    visual_height_ = 0.8f;
    mass_ = 2.0f;
  }
};

class Chest: public IObjModule {
public:
  Chest(const simd_float3& position) {
    name_ = "chest";
    position_ = position;
    half_extent_ = simd_float3{0.5f, 0.5f, 0.55f};
    visual_height_ = 1.0f;
    mass_ = 2.0f;
  }
};

class DirtBlock: public IObjModule {
public:
  DirtBlock(const simd_float3& position) {
    name_ = "dirt_block";
    position_ = position;
    half_extent_ = simd_float3{0.5f, 0.5f, 0.5f};
    visual_height_ = 1.0f;
    mass_ = 3.0f;
  }
};

class Ghast: public IObjModule {
public:
  Ghast(const simd_float3& position) {
    name_ = "ghast";
    position_ = position;
    half_extent_ = simd_float3{1.1f, 2.0f, 1.3f};
    visual_height_ = 4.0f;
    mass_ = 3.0f;
  }
};

class Sheep: public IObjModule {
public:
  Sheep(const simd_float3& position) {
    name_ = "sheep";
    position_ = position;
    half_extent_ = simd_float3{0.4f, 0.7f, 0.75f};
    visual_height_ = 1.4f;
    mass_ = 1.5f;
  }
};

class Steve: public IObjModule {
public:
  Steve(const simd_float3& position) {
    name_ = "steve";
    position_ = position;
    half_extent_ = simd_float3{0.5f, 1.0f, 0.25f};
    visual_height_ = 2.0f;
    mass_ = 2.0f;
  }
};

class Villager: public IObjModule {
public:
  Villager(const simd_float3& position) {
    name_ = "villager";
    position_ = position;
    half_extent_ = simd_float3{0.55f, 1.1f, 0.35f};
    visual_height_ = 2.2f;
    mass_ = 2.0f;
  }
};

class House: public IObjModule {
public:
  House(const simd_float3& position) {
    name_ = "house";
    is_static_ = true;
    affected_by_wind_ = true;
    position_ = position;
    visual_height_ = 4.0f;
    mass_ = 25.0f;
    const float block = visual_height_ / 6.0f;
    half_extent_ = simd_float3{7.5f, 3, 5.5f} * block;
    collision_boxes_ = {
      {simd_float3{0, -2.5f, 0} * block, simd_float3{7.5f, 0.5f, 5.5f} * block},
      {simd_float3{-4.5f, 0, 0} * block, simd_float3{2, 2, 4.5f} * block},
      {simd_float3{2, 0.5f, 0} * block, simd_float3{4.5f, 2.5f, 4.5f} * block}
    };
  }
};

class DirtHouse: public IObjModule {
public:
  DirtHouse(const simd_float3& position) {
    name_ = "dirt_house";
    position_ = position;
    visual_height_ = 3.0f;
    mass_ = 15.0f;
    const float block = visual_height_ / 6.0f;
    half_extent_ = simd_float3{5, 3, 5} * block;
    collision_boxes_ = {
      {simd_float3{0, -2, 0} * block, simd_float3{5, 1, 5} * block},
      {simd_float3{0, 0.5f, 0} * block, simd_float3{3, 1.5f, 3} * block},
      {simd_float3{0, 2.5f, 0} * block, simd_float3{2, 0.5f, 2} * block}
    };
  }
};

class PChair: public IObjModule {
public:
  PChair(const simd_float3& position) {
    name_ = "pchair";
    position_ = position;
    orientation_ = simd_quaternion(0.0f, 1.0f, 0.0f, 0.0f);
    visual_height_ = 2.0f;
    half_extent_ = simd_float3{0.67f, 1.0f, 0.73f};
    mass_ = 0;
    is_static_ = true;
    affected_by_wind_ = false;
  }
};
