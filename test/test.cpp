#include <iostream>

constexpr int square(int x) {
  return x * x;
}

constexpr int x = square(5);

int main() {
  std::cout << x;
  return 0;
}
