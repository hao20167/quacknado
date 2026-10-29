#include "app/application.hpp"
#include <SDL3/SDL.h>
#include <exception>

int main() {
  try {
    quacknado::Application app;
    if (!app.Initialize()) {
      SDL_Log("Quacknado initialization failed!");
      return 1;
    }
    return app.Run();
  } catch (const std::exception &error) {
    SDL_Log("Quacknado failed: %s", error.what());
    return 1;
  }
}
