#include "core/metal_ptr.hpp"
#include <stdexcept>
#include <utility>
namespace {
void check(bool value) {
  if (!value)
    throw std::runtime_error("Metal ownership invariant failed");
}
struct Object {
  int references = 1;
  int *destroyed;
  static inline int retains = 0, releases = 0;
  Object *retain() {
    ++references;
    ++retains;
    return this;
  }
  void release() {
    ++releases;
    if (--references == 0) {
      ++*destroyed;
      delete this;
    }
  }
};
} // namespace
int main() {
  using namespace quacknado::metal;
  int destroyed = 0;
  {
    Ptr<Object> empty;
    auto copy = empty;
    auto moved = std::move(copy);
    empty = empty;
    empty = std::move(empty);
    empty.reset();
    check(!empty && !copy && !moved);
  }
  check(Object::retains == 0 && Object::releases == 0);
  {
    auto a = adopt(new Object{1, &destroyed});
    check(a->references == 1);
    auto b = a;
    check(a->references == 2);
    auto c = std::move(b);
    check(!b && c->references == 2);
    a = a;
    check(a->references == 2);
    a = std::move(a);
    check(a && a->references == 2);
    auto borrowed = retain(a.get());
    check(a->references == 3);
    c = a;
    check(a->references == 3);
    c.reset();
    check(a->references == 2);
    borrowed.reset();
    check(a->references == 1);
    a.reset();
    check(destroyed == 1);
  }
  check(Object::releases == Object::retains + 1);
  {
    auto a = adopt(new Object{1, &destroyed});
    auto b = adopt(new Object{1, &destroyed});
    a = std::move(b);
    check(!b && destroyed == 2);
  }
  check(destroyed == 3);
}
