/* Copyright (c) 2018-2022 Advanced Micro Devices, Inc.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE. */

#ifndef SRC_UTIL_LOGGER_H_
#define SRC_UTIL_LOGGER_H_

#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/file.h>
#include <stdarg.h>
#include <stdlib.h>

#include <atomic>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <exception>
#include <mutex>
#include <map>

namespace roctracer::util {

class Logger {
 public:
  typedef std::recursive_mutex mutex_t;

  template <typename T> Logger& operator<<(const T& m) {
    std::ostringstream oss;
    oss << m;
    if (!streaming_)
      Log(oss.str());
    else
      Put(oss.str());
    streaming_ = true;
    return *this;
  }

  typedef void (*manip_t)();
  Logger& operator<<(manip_t f) {
    f();
    return *this;
  }

  static void begm() { Instance().ResetStreaming(true); }
  static void endl() { Instance().ResetStreaming(false); }

  static const std::string& LastMessage() {
    Logger& logger = Instance();
    std::lock_guard<mutex_t> lck(mutex_);
    return logger.message_[GetTid()];
  }

  static Logger* Create() {
    std::lock_guard<mutex_t> lck(mutex_);
    Logger* obj = instance_.load(std::memory_order_relaxed);
    if (obj == NULL) {
      obj = new Logger();
      if (obj == NULL) {
        std::cerr << "ROCTracer: log object creation failed" << std::endl << std::flush;
        abort();
      }
      instance_.store(obj, std::memory_order_release);
    }
    return obj;
  }

  static void Destroy() {
    std::lock_guard<mutex_t> lck(mutex_);
    if (instance_ != NULL) delete instance_.load();
    instance_ = NULL;
  }

  static Logger& Instance() {
    Logger* obj = instance_.load(std::memory_order_acquire);
    if (obj == NULL) obj = Create();
    return *obj;
  }

  static uint32_t GetPid() { return syscall(__NR_getpid); }
  static uint32_t GetTid() { return syscall(__NR_gettid); }

 private:
  Logger() : file_(NULL), dirty_(false), streaming_(false), messaging_(false) {
    const char* path = getenv("ROCTRACER_LOG");
    if (path != NULL) {
      file_ = fopen("/tmp/roctracer_log.txt", "a");
    }
    ResetStreaming(false);
  }

  ~Logger() {
    if (file_ != NULL) {
      if (dirty_) Put("\n");
      fclose(file_);
    }
  }

  void ResetStreaming(const bool messaging) {
    std::lock_guard<mutex_t> lck(mutex_);
    if (messaging) {
      message_[GetTid()] = "";
    } else if (streaming_) {
      Put("\n");
      dirty_ = false;
    }
    messaging_ = messaging;
    streaming_ = messaging;
  }

  void Put(const std::string& m) {
    std::lock_guard<mutex_t> lck(mutex_);
    if (messaging_) {
      message_[GetTid()] += m;
    }
    if (file_ != NULL) {
      dirty_ = true;
      flock(fileno(file_), LOCK_EX);
      fprintf(file_, "%s", m.c_str());
      fflush(file_);
      flock(fileno(file_), LOCK_UN);
    }
  }

  void Log(const std::string& m) {
    const time_t rawtime = time(NULL);
    tm tm_info;
    localtime_r(&rawtime, &tm_info);
    char tm_str[26];
    strftime(tm_str, 26, "%Y-%m-%d %H:%M:%S", &tm_info);
    std::ostringstream oss;
    oss << "<" << tm_str << std::dec << " pid" << GetPid() << " tid" << GetTid() << "> " << m;
    Put(oss.str());
  }

  FILE* file_;
  bool dirty_;
  bool streaming_;
  bool messaging_;

  static mutex_t mutex_;
  static std::atomic<Logger*> instance_;
  std::map<uint32_t, std::string> message_;
};

}  // namespace roctracer::util

#define FATAL_LOGGING(stream)                                                                      \
  do {                                                                                             \
    roctracer::util::Logger::Instance()                                                            \
        << "fatal: " << roctracer::util::Logger::begm << stream << roctracer::util::Logger::endl;  \
    abort();                                                                                       \
  } while (0)

#define ERR_LOGGING(stream)                                                                        \
  do {                                                                                             \
    roctracer::util::Logger::Instance()                                                            \
        << "error: " << roctracer::util::Logger::begm << stream << roctracer::util::Logger::endl;  \
  } while (0)

#define INFO_LOGGING(stream)                                                                       \
  do {                                                                                             \
    roctracer::util::Logger::Instance()                                                            \
        << "info: " << roctracer::util::Logger::begm << stream << roctracer::util::Logger::endl;   \
  } while (0)

#define WARN_LOGGING(stream)                                                                       \
  do {                                                                                             \
    std::cerr << "ROCProfiler: " << stream << std::endl;                                           \
    roctracer::util::Logger::Instance() << "warning: " << roctracer::util::Logger::begm << stream  \
                                        << roctracer::util::Logger::endl;                          \
  } while (0)

#ifdef DEBUG
#define DBG_LOGGING(stream)                                                                        \
  do {                                                                                             \
    roctracer::util::Logger::Instance()                                                            \
        << roctracer::util::Logger::begm << "debug: \"" << stream << "\""                          \
        << " in " << __FUNCTION__ << " at " << __FILE__ << " line " << __LINE__                    \
        << roctracer::util::Logger::endl;                                                          \
  } while (0)
#endif

#if DEBUG_TRACE_ON
inline static void DEBUG_TRACE(const char* fmt, ...) {
  constexpr int size = 256;
  char buf[size];

  va_list valist;
  va_start(valist, fmt);
  vsnprintf(buf, size, fmt, valist);
  printf("%u:%u %s", roctracer::util::Logger::GetPid(), roctracer::util::Logger::GetTid(), buf);
  fflush(stdout);
  va_end(valist);
}
#else
#define DEBUG_TRACE(...)
#endif

#endif  // SRC_UTIL_LOGGER_H_
