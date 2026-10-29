#pragma once
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

// Synchronous parallel ranges. One owner calls run; tasks must not recurse into run.
namespace quacknado {
class WorkerPool {
  std::mutex mutex_;
  std::condition_variable ready_, done_;
  std::vector<std::thread> threads_;
  std::function<void(size_t, size_t)> fn_;
  size_t generation_ = 0, pending_ = 0, count_ = 0;
  bool stop_ = false;
  std::exception_ptr error_;

public:
  explicit WorkerPool(size_t workers) {
    if (!workers)
      throw std::invalid_argument("Pool needs at least one worker");
    try {
      for (size_t id = 0; id < workers; ++id)
        threads_.emplace_back([this, id, workers] {
          size_t seen = 0;
          std::unique_lock lock(mutex_);
          for (;;) {
            ready_.wait(lock, [&] { return stop_ || generation_ != seen; });
            if (stop_)
              return;
            seen = generation_;
            size_t n = count_;
            lock.unlock();
            try {
              fn_(n * id / workers, n * (id + 1) / workers);
            } catch (...) {
              std::lock_guard guard(mutex_);
              if (!error_)
                error_ = std::current_exception();
            }
            lock.lock();
            if (--pending_ == 0)
              done_.notify_one();
          }
        });
    } catch (...) {
      {
        std::lock_guard lock(mutex_);
        stop_ = true;
      }
      ready_.notify_all();
      for (auto &t : threads_)
        t.join();
      throw;
    }
  }
  WorkerPool(const WorkerPool &) = delete;
  WorkerPool &operator=(const WorkerPool &) = delete;
  ~WorkerPool() {
    {
      std::lock_guard lock(mutex_);
      stop_ = true;
    }
    ready_.notify_all();
    for (auto &t : threads_)
      t.join();
  }
  void run(size_t n, std::function<void(size_t, size_t)> fn) {
    std::unique_lock lock(mutex_);
    error_ = nullptr;
    fn_ = std::move(fn);
    count_ = n;
    pending_ = threads_.size();
    ++generation_;
    ready_.notify_all();
    done_.wait(lock, [&] { return pending_ == 0; });
    if (error_)
      std::rethrow_exception(error_);
  }
};
} // namespace quacknado
