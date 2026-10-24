#pragma once

#include <memory>

#include "env/tornado.hpp"
#include "objs/duck.hpp"

class Simulation {
private:
  std::unique_ptr<Tornado> tornado_ = std::make_unique<Tornado>();
public:
  std::unique_ptr<Duck> duck_ = std::make_unique<Duck>();
  void update(const float& dt);
};
