#ifndef LOGGING_H_
#define LOGGING_H_

#include <mutex>
#include <sstream>
#include <string>

enum class LogLevel {
  DEBUG = 0,
  INFO,
  WARN,
  ERROR,
};

class Logger {
public:
  static Logger &instance();

  void setLevel(LogLevel level);
  LogLevel level() const;

  void log(LogLevel level, const char *file, int line, const std::string &msg);

private:
  Logger() = default;
  ~Logger() = default;

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

private:
  LogLevel level_ = LogLevel::DEBUG;
  mutable std::mutex mutex_;
};

const char *logLevelToString(LogLevel level);

#define LOG_STREAM(level, expr)                                                \
  do {                                                                         \
    std::ostringstream _log_oss;                                               \
    _log_oss << expr;                                                          \
    Logger::instance().log(level, __FILE__, __LINE__, _log_oss.str());         \
  } while (0)

#define LOG_DEBUG(expr) LOG_STREAM(LogLevel::DEBUG, expr)
#define LOG_INFO(expr) LOG_STREAM(LogLevel::INFO, expr)
#define LOG_WARN(expr) LOG_STREAM(LogLevel::WARN, expr)
#define LOG_ERROR(expr) LOG_STREAM(LogLevel::ERROR, expr)

#endif