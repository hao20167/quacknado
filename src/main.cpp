#include <SDL3/SDL.h>

#include "app/application.hpp"

int main() {
  quacknado::Application app;
  if (!app.Initialize()) {
    SDL_Log("Ducknado initialization failed!\n");
    return 1;
  }

  return app.Run();

  return 0;
}
