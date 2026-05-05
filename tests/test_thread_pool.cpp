#include "Thread_pool.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  std::atomic<int> counter{0};

  ThreadPool pool(4);

  for (int i = 0; i < 1000; ++i) {
    bool ok = pool.addTask([&counter]() {
      counter.fetch_add(1);
    });
    assert(ok);
  }

  pool.shutdown();

  bool rejected = !pool.addTask([]() {});
  assert(rejected);

  pool.stop();

  assert(counter.load() == 1000);

  std::cout << "test_thread_pool passed\n";
  return 0;
}