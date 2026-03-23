#include "logging.h"
#include <thread>
#include <vector>

int main() {
  Logger::instance().setLevel(LogLevel::DEBUG);

  LOG_DEBUG("debug message");
  LOG_INFO("info message");
  LOG_WARN("warn message");
  LOG_ERROR("error message");

  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back([i]() {
      for (int j = 0; j < 5; ++j) {
        LOG_INFO("worker=" << i << ", j=" << j);
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  return 0;
}