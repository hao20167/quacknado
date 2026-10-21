#pragma once

#include <cmath>

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;

  constexpr Vec3() = default;
  constexpr Vec3(float x, float y, float z): x(x), y(y), z(z) {}

  float length() const {
    return std::sqrt(x * x + y * y + z * z);
  }

  constexpr Vec3 operator+(const Vec3& other) const {
    return Vec3(x + other.x, y + other.y, z + other.z);
  }

  constexpr Vec3 operator-(const Vec3& other) const {
    return Vec3(x - other.x, y - other.y, z + other.z);
  }

  constexpr Vec3 operator*(const float& scalar) const {
    return Vec3(x * scalar, y * scalar, z * scalar);
  }

  constexpr Vec3& operator+=(const Vec3& other) {
    x += other.x; y += other.y; z += other.z;
    return *this;
  }

};

// dp/dt = F
// v = p/m
// dx/dt = v
struct State {
  // primary
  Vec3 position;
  Vec3 momentum;
  // secondary
  Vec3 velocity;
  // constant
  float mass;
  float inverse_mass;

  void Recalculate() {
    velocity = momentum * inverse_mass;
  }
};

struct Derivative {
  Vec3 velocity;
  Vec3 force;
};
