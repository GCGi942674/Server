#include "logging.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string.h>
#include <thread>

namespace {
const char *baseName(const char *path) {
  if (path == nullptr) {
    return "";
  }

  const char *slash1 = strrchr(path, '/');
  const char *slash2 = strrchr(path, '\\');
  const char *pos = slash1 > slash2 ? slash1 : slash2;
  return pos ? pos + 1 : path;
}

std::string nowString() {
  using namespace std::chrono;

  auto now = system_clock::now();
  auto tt = system_clock::to_time_t(now);
  auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  std::tm tm_time{};
#if defined(_WIN32)
  localtime_s(&tm_time, &tt);
#else
  localtime_r(&tt, &tm_time);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm_time, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3)
      << std::setfill('0') << ms.count();
  return oss.str();
}

} // namespace

Logger &Logger::instance() {
  static Logger logger;
  return logger;
}

LogLevel Logger::level() const {
  std::lock_guard<std::mutex> lock(this->mutex_);
  return this->level_;
}

void Logger::log(LogLevel level, const char *file, int line,
                 const std::string &msg) {
  LogLevel current_level;
  {
    std::lock_guard<std::mutex> lock(this->mutex_);
    current_level = this->level_;
  }
  if (static_cast<int>(level) < static_cast<int>(current_level)) {
    return;
  }
  std::ostringstream oss;
  oss << '[' << nowString() << ']' << '[' << logLevelToString(level) << ']'
      << "[tid:" << std::this_thread::get_id() << ']' << '[' << baseName(file)
      << ':' << line << "]" << msg << '\n';

  std::lock_guard<std::mutex> lock(this->mutex_);
  if (level == LogLevel::ERROR || level == LogLevel::WARN) {
    std::cerr << oss.str();
    std::cerr.flush();
  } else {
    std::cout << oss.str();
    std::cout.flush();
  }
}

const char *logLevelToString(LogLevel level) {
  switch (level) {
  case LogLevel::DEBUG:
    return "DEBUG";
  case LogLevel::INFO:
    return "INFO";
  case LogLevel::WARN:
    return "WARN";
  case LogLevel::ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
};

void Logger::setLevel(LogLevel level) {
  std::lock_guard<std::mutex> lock(this->mutex_);
  this->level_ = level;
}