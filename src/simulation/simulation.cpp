#include "simulation.hpp"

#include "env/tornado.hpp"

void Simulation::update(const float& dt) {
  tornado_->UpdateObj(duck_.get(), dt);
}
