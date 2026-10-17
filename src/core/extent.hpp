#pragma once

namespace quacknado {

struct Extent2D {
  int width, height;

  [[nodiscard]]
  constexpr bool IsValid() const noexcept {
    return width > 0 && height > 0;
  }
};

}
