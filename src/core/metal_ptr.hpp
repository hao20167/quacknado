#pragma once
#include <cstddef>
#include <utility>

// Intrusive ownership for metal-cpp objects. A null pointer never receives a
// C++ member call, even though Objective-C itself accepts messaging nil.
namespace quacknado::metal {
template <class T> class Ptr {
  T *pointer_ = nullptr;
  explicit Ptr(T *pointer) noexcept : pointer_(pointer) {}

public:
  Ptr() noexcept = default;
  Ptr(std::nullptr_t) noexcept {}
  static Ptr adopt(T *pointer) noexcept { return Ptr(pointer); }
  static Ptr retain(T *pointer) noexcept {
    if (pointer)
      pointer->retain();
    return Ptr(pointer);
  }
  Ptr(const Ptr &other) noexcept : Ptr(retain(other.pointer_)) {}
  Ptr(Ptr &&other) noexcept : pointer_(std::exchange(other.pointer_, nullptr)) {}
  ~Ptr() {
    if (pointer_)
      pointer_->release();
  }
  Ptr &operator=(Ptr other) noexcept {
    swap(other);
    return *this;
  }
  void swap(Ptr &other) noexcept { std::swap(pointer_, other.pointer_); }
  void reset() noexcept { Ptr{}.swap(*this); }
  T *get() const noexcept { return pointer_; }
  T *operator->() const noexcept { return pointer_; }
  explicit operator bool() const noexcept { return pointer_ != nullptr; }
};
template <class T> Ptr<T> adopt(T *pointer) noexcept { return Ptr<T>::adopt(pointer); }
template <class T> Ptr<T> retain(T *pointer) noexcept { return Ptr<T>::retain(pointer); }
} // namespace quacknado::metal
